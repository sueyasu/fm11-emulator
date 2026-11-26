#pragma once
#include "Machine.h"
#include "Main6809Board.h"
#include "Main8088Board.h"
#include "MainZ80Board.h"
#include "Sim09.h"
#include "SimZ80.h"
#include <fstream>
#include <cstdio>
#include <atomic>
#include <mutex>
#include <set>
#include <tuple>
#include <array>
#include <memory>
#include <cstdint>
#include <stdexcept>
#include <utility>
#include <functional>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

// Driven only on SDL's event thread. All guest workers must be stopped
// before CPU state inspection. The input reader never touches guest state.
class DebugMonitor {
public:
    using PauseFn=std::function<void(bool)>;
    DebugMonitor(Machine& machine,PauseFn pause):m_(machine),pause_(std::move(pause)) {
        selected_ = m_.activeBoard()?name(*m_.activeBoard()):"sub";
        for(std::size_t i=0;i<bitmaps_.size();++i) {
            bitmaps_[i]=std::make_unique<std::atomic<std::uint64_t>[]>(wordCounts_[i]);
            for(std::size_t w=0;w<wordCounts_[i];++w)
                bitmaps_[i][w].store(0,std::memory_order_relaxed);
            watchReads_[i]=std::make_unique<std::atomic<std::uint64_t>[]>(wordCounts_[i]);
            watchWrites_[i]=std::make_unique<std::atomic<std::uint64_t>[]>(wordCounts_[i]);
            for(std::size_t w=0;w<wordCounts_[i];++w) {
                watchReads_[i][w].store(0,std::memory_order_relaxed);
                watchWrites_[i][w].store(0,std::memory_order_relaxed);
            }
        }
        for(std::size_t i=0;i<m_.boardCount();++i) {
            auto& board=m_.board(i);
            if(auto* p=dynamic_cast<Main6809Board*>(&board))
                p->setMemoryAccessObserver([this](WORD addr,bool write,BYTE value) {
                    watchProbe(Target::Main6809,addr,write,value);
                });
            else if(auto* p=dynamic_cast<Main8088Board*>(&board))
                p->bus().setMemoryAccessObserver([this](std::uint32_t addr,bool write,BYTE value) {
                    watchProbe(Target::Main8088,addr&0xfffff,write,value);
                });
            else if(auto* p=dynamic_cast<MainZ80Board*>(&board))
                p->setMemoryAccessObserver([this](WORD addr,bool write,BYTE value) {
                    watchProbe(Target::Z80,addr,write,value);
                });
        }
    }
    ~DebugMonitor() {
        for(std::size_t i=0;i<m_.boardCount();++i) {
            auto& board=m_.board(i);
            if(auto* p=dynamic_cast<Main6809Board*>(&board))
                p->setMemoryAccessObserver({});
            else if(auto* p=dynamic_cast<Main8088Board*>(&board))
                p->bus().setMemoryAccessObserver({});
            else if(auto* p=dynamic_cast<MainZ80Board*>(&board))
                p->setMemoryAccessObserver({});
        }
    }
    enum class Target : std::uint8_t { Main6809, Main8088, Z80, Sub };
    static Target cpuTarget(CpuType type) {
        switch(type) {
            case CpuType::MC6809: case CpuType::HD6309:return Target::Main6809;
            case CpuType::I8088: case CpuType::NECV20:return Target::Main8088;
            case CpuType::Z80:return Target::Z80;
        }
        return Target::Main6809;
    }
    // The emulator hot path uses only fixed-width integers and atomic operations.
    // No string creation, set lookup or mutex acquisition is required.
    bool probe(Target cpu,std::uint32_t pc) {
        const auto pending=hit_.load(std::memory_order_acquire);
        if(pending)return true;
        if(!hasBreaks_.load(std::memory_order_relaxed))return false;
        if(pc>=limit(cpu))return false;
        const auto& bits=bitmaps_[index(cpu)];
        const auto word=bits[pc>>6].load(std::memory_order_relaxed);
        if(!(word&(std::uint64_t{1}<<(pc&63))))return false;
        const auto encoded=encode(cpu,pc);
        auto skip=encoded;
        if(skip_.compare_exchange_strong(skip,0,std::memory_order_acq_rel))
            return false;
        std::uint64_t none=0;
        hit_.compare_exchange_strong(none,encoded,std::memory_order_acq_rel);
        return true;
    }
    void watchProbe(Target cpu,std::uint32_t addr,bool write,BYTE value) {
        if(!hasWatches_.load(std::memory_order_relaxed) ||
           hit_.load(std::memory_order_relaxed)!=0 || addr>=limit(cpu))return;
        const auto& bits=(write?watchWrites_:watchReads_)[index(cpu)];
        const auto word=bits[addr>>6].load(std::memory_order_relaxed);
        if(!(word&(std::uint64_t{1}<<(addr&63))))return;
        const std::uint64_t event=(std::uint64_t{1}<<63) |
            (std::uint64_t(write)<<40) | (std::uint64_t(value)<<32) |
            encode(cpu,addr);
        std::uint64_t none=0;
        hit_.compare_exchange_strong(none,event,std::memory_order_acq_rel);
    }
    bool breakPending() const {return hit_.load(std::memory_order_acquire)!=0;}
    // Cheap guard for instruction loops. Avoid reading CPU type or PC when
    // no execution breakpoint is configured and no stop is pending.
    bool needsProbe() const {
        return hasBreaks_.load(std::memory_order_relaxed) ||
               hit_.load(std::memory_order_acquire)!=0;
    }
    // Only called after both main and display workers are paused.
    std::string takeBreak() {
        const auto hit=hit_.exchange(0,std::memory_order_acq_rel);
        if(!hit)return {};
        const auto cpu=decodeCpu(hit);
        const auto pc=decodePc(hit);
        if(hit&(std::uint64_t{1}<<63)) {
            stoppedAt_=0;
            const bool write=(hit&(std::uint64_t{1}<<40))!=0;
            const auto value=static_cast<BYTE>((hit>>32)&0xff);
            return "Watchpoint: "+targetName(cpu)+(write?" write ":" read ")+
                hex(pc,cpu==Target::Main8088?5:4)+" value="+hex(value,2)+"\n";
        }
        stoppedAt_=hit;
        return "Breakpoint: "+targetName(cpu)+" PC="+hex(pc,cpu==Target::Main8088?5:4)+"\n";
    }
    std::string prompt() const {return "fm11["+selected_+"]> ";}
    void emit(const std::string& s) {
        std::cout<<s<<std::flush;
        if(log_.is_open())log_<<s<<std::flush;
    }
    void recordInput(const std::string& s) {
        if(log_.is_open())log_<<prompt()<<s<<"\n"<<std::flush;
    }
    std::string execute(const std::string& line,bool& paused) {
        std::istringstream input(line);
        std::vector<std::string> args;
        for(std::string token;input>>token;)args.push_back(token);
        if(args.empty())return {};
        const auto& command=args[0];
        try {
            if(command=="help" || command=="?")return
                "help, info cpus, info machine, info io FDXX, cpu NAME, stop, cont, step [COUNT], regs, "
                "x HEXADDR [HEXLEN], xp HEXADDR [HEXLEN], dis [HEXADDR] [COUNT], "
                "b HEXADDR, bl, bc [HEXADDR], w r|w|rw HEXADDR, wl, wc [HEXADDR], "
                "log start FILE, log stop, log status\n";
            if(command=="log") {
                if(args.size()==2 && args[1]=="status")
                    return log_.is_open()?"Logging: "+file_+"\n":"Logging: off\n";
                if(args.size()==2 && args[1]=="stop") {
                    if(!log_.is_open())return "Logging is not active.\n";
                    const auto path=file_;
                    log_<<"Logging stopped: "<<path<<"\n";
                    log_.close();file_.clear();
                    return "Logging stopped: "+path+"\n";
                }
                if(args.size()==3 && args[1]=="start") {
                    if(log_.is_open())return "Already logging to "+file_+"\n";
                    log_.open(args[2],std::ios::app);
                    if(!log_) {log_.clear();return "Cannot open log: "+args[2]+"\n";}
                    file_=args[2];
                    return "Logging started (append): "+file_+"\n";
                }
                return "Usage: log start FILE | log stop | log status\n";
            }
            if(command=="w" || command=="watch") {
                if(args.size()!=3 || (args[1]!="r" && args[1]!="w" && args[1]!="rw"))
                    return "Usage: w r|w|rw HEXADDR\n";
                const auto cpu=parseTarget(selected_);
                if(cpu==Target::Sub)return "Sub-CPU watchpoints are not yet supported.\n";
                const auto addr=address(args[2]);
                if(addr>=limit(cpu))return "Watchpoint address out of range.\n";
                std::lock_guard<std::mutex> lock(breakMutex_);
                for(const char mode:args[1]) {
                    if(!watches_.insert({selected_,mode,static_cast<std::uint32_t>(addr)}).second)
                        continue;
                    auto& bits=(mode=='w'?watchWrites_:watchReads_)[index(cpu)];
                    bits[addr>>6].fetch_or(std::uint64_t{1}<<(addr&63),std::memory_order_release);
                }
                hasWatches_.store(!watches_.empty(),std::memory_order_release);
                return "Watchpoint set: "+selected_+" "+args[1]+" "+hex(addr,cpu==Target::Main8088?5:4)+"\n";
            }
            if(command=="wl") {
                if(args.size()!=1)return "Usage: wl\n";
                std::lock_guard<std::mutex> lock(breakMutex_);
                std::ostringstream out;
                for(const auto& entry:watches_) {
                    const auto& name=std::get<0>(entry);
                    out<<name<<" "<<std::get<1>(entry)<<" "<<hex(std::get<2>(entry),name=="main8088"?5:4)<<"\n";
                }
                return out.str().empty()?"No watchpoints.\\n":out.str();
            }
            if(command=="wc") {
                if(args.size()>2)return "Usage: wc [HEXADDR]\n";
                const auto cpu=parseTarget(selected_);
                std::uint64_t addr=0;
                if(args.size()==2) {
                    addr=address(args[1]);
                    if(addr>=limit(cpu))return "Watchpoint address out of range.\n";
                }
                std::lock_guard<std::mutex> lock(breakMutex_);
                for(auto it=watches_.begin();it!=watches_.end();) {
                    if(std::get<0>(*it)!=selected_ ||
                       (args.size()==2 && std::get<2>(*it)!=addr)) {++it;continue;}
                    const char mode=std::get<1>(*it);
                    const auto a=std::get<2>(*it);
                    auto& bits=(mode=='w'?watchWrites_:watchReads_)[index(cpu)];
                    bits[a>>6].fetch_and(~(std::uint64_t{1}<<(a&63)),std::memory_order_release);
                    it=watches_.erase(it);
                }
                hasWatches_.store(!watches_.empty(),std::memory_order_release);
                return "Watchpoint(s) cleared for "+selected_+".\n";
            }
            if(command=="b" || command=="break") {
                if(args.size()!=2)return "Usage: b HEXADDR\n";
                const auto addr=address(args[1]);
                const auto cpu=parseTarget(selected_);
                if(addr>=limit(cpu))return "Breakpoint address out of range.\n";
                {
                    std::lock_guard<std::mutex> lock(breakMutex_);
                    if(breaks_.insert({selected_,static_cast<std::uint32_t>(addr)}).second) {
                        bitmaps_[index(cpu)][addr>>6].fetch_or(
                            std::uint64_t{1}<<(addr&63),std::memory_order_release);
                        ++breakCount_;
                        hasBreaks_.store(true,std::memory_order_release);
                    }
                }
                return "Breakpoint set: "+selected_+" PC="+hex(addr,cpu==Target::Main8088?5:4)+"\n";
            }
            if(command=="bl") {
                if(args.size()!=1)return "Usage: bl\n";
                std::lock_guard<std::mutex> lock(breakMutex_);
                std::ostringstream out;
                for(const auto& entry:breaks_)
                    out<<entry.first<<" PC="<<hex(entry.second,entry.first=="main8088"?5:4)<<"\n";
                return out.str().empty()?"No breakpoints.\n":out.str();
            }
            if(command=="bc") {
                if(args.size()>2)return "Usage: bc [HEXADDR]\n";
                std::lock_guard<std::mutex> lock(breakMutex_);
                const auto cpu=parseTarget(selected_);
                auto remove=[&](std::uint32_t addr) {
                    if(breaks_.erase({selected_,addr})) {
                        bitmaps_[index(cpu)][addr>>6].fetch_and(
                            ~(std::uint64_t{1}<<(addr&63)),std::memory_order_release);
                        --breakCount_;
                    }
                };
                if(args.size()==1) {
                    for(auto it=breaks_.begin();it!=breaks_.end();) {
                        if(it->first==selected_) {
                            const auto addr=it->second;
                            it=breaks_.erase(it);
                            bitmaps_[index(cpu)][addr>>6].fetch_and(
                                ~(std::uint64_t{1}<<(addr&63)),std::memory_order_release);
                            --breakCount_;
                        } else ++it;
                    }
                } else {
                    const auto addr=address(args[1]);
                    if(addr>=limit(cpu))return "Breakpoint address out of range.\n";
                    remove(static_cast<std::uint32_t>(addr));
                }
                hasBreaks_.store(breakCount_!=0,std::memory_order_release);
                return "Breakpoint(s) cleared for "+selected_+".\n";
            }
            if(command=="info" && args.size()==2 && args[1]=="cpus") {
                std::ostringstream out;
                for(std::size_t i=0;i<m_.boardCount();++i) {
                    auto& b=m_.board(i);
                    out<<name(b)<<"  "<<type(b.cpu().type())
                       <<(m_.activeBoard()==&b?" ACTIVE":" INACTIVE")
                       <<(selected_==name(b)?" SELECTED":"")<<"\n";
                }
                out<<"sub  "<<(m_.systemBoard().displayCpu().is6309Enabled()?"HD6309":"MC6809")
                   <<" INDEPENDENT"<<(selected_=="sub"?" SELECTED":"")<<"\n";
                return out.str();
            }
            if(command=="info" && args.size()==2 && args[1]=="machine") {
                pause_(true);paused=true;
                std::ostringstream out;
                out<<"Active main CPU: "<<type(m_.activeCpuType())<<"\n";
                for(std::size_t i=0;i<m_.boardCount();++i) {
                    auto& b=m_.board(i);
                    out<<name(b)<<" ("<<type(b.cpu().type())<<")";
                    if(m_.activeBoard()==&b)out<<" ACTIVE";
                    if(auto* p=dynamic_cast<Main6809Board*>(&b)) {
                        out<<" RAM="<<p->ramKiB()<<" KiB"
                           <<" IRQ="<<(p->irqPending()?"pending":"clear")
                           <<" IRQ-accepted="<<p->acceptedIrqCount()
                           <<" FDC-status="<<hex(p->fdcStatus(),2)
                           <<" FDC-track="<<hex(p->fdcTrack(),2)
                           <<" FDC-sector="<<hex(p->fdcSector(),2);
                    } else if(auto* p=dynamic_cast<Main8088Board*>(&b)) {
                        out<<" RAM="<<p->bus().ramKiB()<<" KiB"
                           <<" PIC-mask="<<hex(p->bus().interruptMask(),2)
                           <<" DMA-mask="<<hex(p->bus().dmaMask(),2);
                    } else if(auto* p=dynamic_cast<MainZ80Board*>(&b)) {
                        out<<" RAM="<<p->ramKiB()<<" KiB"
                           <<" bank="<<hex(p->bank(),2);
                    }
                    out<<"\n";
                }
                return out.str();
            }
            if(command=="info" && args.size()==3 && args[1]=="io") {
                // Non-invasive counters and write latch; never call readIo().
                const auto value=number(args[2]);
                if(value<0xfd00 || value>0xfdff)
                    return "Usage: info io FD00..FDFF (6809 I/O counters)\n";
                pause_(true);paused=true;
                auto* b=board("main6809");
                auto* p=dynamic_cast<Main6809Board*>(b);
                if(!p)return "6809 board is not installed.\n";
                const auto reg=static_cast<unsigned>(value&0xffu);
                std::ostringstream out;
                out<<"FD"<<hex(reg,2)
                   <<" reads="<<p->ioReadCount(reg)
                   <<" writes="<<p->ioWriteCount(reg)
                   <<" last-write="<<hex(p->lastIoWrite(reg),2)<<"\n";
                return out.str();
            }
            if(command=="cpu") {
                if(args.size()!=2)return "Usage: cpu main6809|main8088|z80|sub\n";
                if(args[1]!="sub" && !board(args[1]))return "CPU not installed: "+args[1]+"\n";
                selected_=args[1];return "Selected: "+selected_+"\n";
            }
            if(command=="stop") {pause_(true);paused=true;return "Emulation stopped.\n";}
            if(command=="cont" || command=="c") {
                // Exactly one instruction at the last stopped PC is allowed.
                skip_.store(stoppedAt_,std::memory_order_release);
                stoppedAt_=0;
                pause_(false);paused=false;
                return "Emulation running.\n";
            }
            if(command=="r" || command=="regs") {
                pause_(true);paused=true;return registers();
            }
            if(command=="s" || command=="step") {
                if(args.size()>2)return "Usage: step [COUNT]\n";
                const auto n=args.size()==2?count(args[1]):1;
                if(n==0 || n>10000)return "Step count must be 1..10000.\n";
                pause_(true);paused=true;
                if(selected_=="sub") {
                    auto lock=m_.lockDisplay();
                    for(unsigned i=0;i<n;++i)
                        if(!m_.systemBoard().displayCpu().executeOne())break;
                } else {
                    auto* target=board(selected_);
                    if(!target || m_.activeBoard()!=target)
                        return "CPU is not the active execution owner.\n";
                    for(unsigned i=0;i<n;++i)m_.executeOne();
                }
                // Print the instruction at the new PC after a step.
                // This is inspection only and cannot advance CPU state.
                const auto result=registers();
                return result+externalDisassemble(disArchitecture(),disPc(),1);
            }
            if(command=="dis" || command=="u") {
                if(args.size()>3)return "Usage: dis [HEXADDR] [COUNT]\n";
                const auto amount=args.size()>2?count(args[2]):8;
                if(amount<1 || amount>64)return "Instruction count must be 1..64.\n";
                // Stop all workers before sampling the selected CPU's PC.
                pause_(true);paused=true;
                const auto cpu=disArchitecture();
                const auto pc=args.size()>1?address(args[1]):disPc();
                if(pc>=disLimit())return "Disassembly address out of range.\n";
                return externalDisassemble(cpu,static_cast<std::uint32_t>(pc),static_cast<unsigned>(amount));
            }
            if(command=="x" || command=="xp") {
                if(args.size()<2 || args.size()>3)return "Usage: x|xp HEXADDR [HEXLEN]\n";
                auto addr=address(args[1]),len=args.size()==3?count(args[2]):128;
                const bool physical=command=="xp";
                const auto limit=(physical || selected_=="main8088")?0x100000u:0x10000u;
                if(!len || len>4096 || addr>=limit || len>limit-addr)
                    return "Invalid range (maximum 4096 bytes).\n";
                pause_(true);paused=true;
                auto lock=m_.lockDisplay();
                std::ostringstream out;
                for(std::uint64_t i=0;i<len;++i) {
                    if(i%16==0)out<<hex(addr+i,limit>65536?5:4)<<":";
                    out<<" "<<hex(peek(addr+i,physical),2);
                    if(i%16==15 || i+1==len)out<<"\n";
                }
                return out.str();
            }
            return "Unknown command: "+command+"\n";
        } catch(const std::exception& e) {
            return std::string("Monitor error: ")+e.what()+"\n";
        }
    }
private:
    static std::string hex(std::uint64_t n,int width) {
        std::ostringstream out;out<<std::uppercase<<std::hex
            <<std::setfill('0')<<std::setw(width)<<n;return out.str();
    }
    static std::uint64_t number(const std::string& s) {
        std::size_t at=0;auto n=std::stoull(s,&at,16);
        if(at!=s.size())throw std::invalid_argument("expected hexadecimal number");
        return n;
    }
    static std::uint64_t count(const std::string& s) {
        std::size_t at=0;auto n=std::stoull(s,&at,0);
        if(at!=s.size())throw std::invalid_argument("expected decimal count or 0x-prefixed hexadecimal");
        return n;
    }
    std::uint64_t address(const std::string& s) const {
        const auto colon=s.find(':');
        if(colon==std::string::npos)return number(s);
        if(selected_!="main8088")
            throw std::invalid_argument("segment:offset requires main8088");
        const auto seg=number(s.substr(0,colon));
        const auto off=number(s.substr(colon+1));
        if(seg>0xffff || off>0xffff)
            throw std::out_of_range("segment and offset must be 16-bit");
        return ((seg<<4)+off)&0xfffff;
    }
    static const char* type(CpuType t) {
        switch(t) {
            case CpuType::MC6809:return "MC6809";case CpuType::HD6309:return "HD6309";
            case CpuType::I8088:return "8088";case CpuType::NECV20:return "V20";
            case CpuType::Z80:return "Z80";
        }
        return "?";
    }
    static std::string name(const CpuBoard& b) {
        switch(b.cpu().type()) {
            case CpuType::MC6809:case CpuType::HD6309:return "main6809";
            case CpuType::I8088:case CpuType::NECV20:return "main8088";
            case CpuType::Z80:return "z80";
        }
        return "?";
    }
    CpuBoard* board(const std::string& target) const {
        for(std::size_t i=0;i<m_.boardCount();++i)
            if(name(m_.board(i))==target)return &m_.board(i);
        return nullptr;
    }
    std::string registers() const {
        std::ostringstream out;
        auto show6809=[&](const CSim09& c) {
            const auto& r=c.regs;
            out<<"PC="<<hex(r.r.w.PC,4)<<" S="<<hex(r.r.w.S,4)
               <<" U="<<hex(r.r.w.U,4)<<" X="<<hex(r.r.w.X,4)
               <<" Y="<<hex(r.r.w.Y,4)<<" D="<<hex(r.r.w.D,4)
               <<" DP="<<hex(r.DP,2)<<" CC="<<hex(r.CC,2);
            if(c.Is6309Enabled())out<<" E="<<hex(r.r.b.E,2)
               <<" F="<<hex(r.r.b.F,2)<<" W="<<hex(r.r.w.W,4)
               <<" V="<<hex(r.r.w.V,4)<<" MD="<<hex(r.MD,2);
            out<<"\n";
        };
        if(selected_=="sub") {
            auto guard=m_.lockDisplay();
            show6809(m_.systemBoard().displayCpu());
        } else if(auto* b=board(selected_)) {
            const auto& c=b->cpu();
            if(auto* p=dynamic_cast<const CSim09*>(&c))show6809(*p);
            else if(auto* p=dynamic_cast<const Main8088Board::Core*>(&c)) {
                out<<"CS:IP="<<hex(p->cs(),4)<<":"<<hex(p->ip(),4)
                   <<" FLAGS="<<hex(p->flags(),4);
                const char* regs[]={"AX","CX","DX","BX","SP","BP","SI","DI"};
                for(int i=0;i<8;++i)out<<" "<<regs[i]<<"="<<hex(p->reg(i),4);
                const char* segs[]={"ES","CS","SS","DS"};
                for(int i=0;i<4;++i)out<<" "<<segs[i]<<"="<<hex(p->segment(i),4);
                out<<" 8080="<<(p->emulation8080()?"on":"off")<<"\n";
            } else if(auto* p=dynamic_cast<const CSimZ80*>(&c)) {
                const auto& r=p->regs;
                out<<"PC="<<hex(r.PC,4)<<" SP="<<hex(r.SP,4)
                   <<" AF="<<hex(r.r.w.AF,4)<<" BC="<<hex(r.r.w.BC,4)
                   <<" DE="<<hex(r.r.w.DE,4)<<" HL="<<hex(r.r.w.HL,4)
                   <<" IX="<<hex(r.IX,4)<<" IY="<<hex(r.IY,4)
                   <<" AF'="<<hex(r.r.w.AFs,4)<<" BC'="<<hex(r.r.w.BCs,4)
                   <<" DE'="<<hex(r.r.w.DEs,4)<<" HL'="<<hex(r.r.w.HLs,4)
                   <<" I="<<hex(r.r.b.I,2)<<" R="<<hex(r.r.b.R,2)
                   <<" IFF1="<<unsigned(r.f.IFF1)<<" IFF2="<<unsigned(r.f.IFF2)<<"\n";
            }
        }
        return out.str();
    }
    // Architecture selection only. Decoding is delegated to external tools.
    std::string disArchitecture() const {
        if(selected_=="sub")
            return m_.systemBoard().displayCpu().is6309Enabled()?"hd6309":"m6809";
        const auto* b=board(selected_);
        if(!b)throw std::runtime_error("CPU not installed");
        switch(b->cpu().type()) {
            case CpuType::MC6809:return "m6809";
            case CpuType::HD6309:return "hd6309";
            case CpuType::Z80:return "z80";
            case CpuType::I8088:return "i8088";
            case CpuType::NECV20:
                if(const auto* v20=dynamic_cast<const Main8088Board::Core*>(&b->cpu()))
                    return v20->emulation8080()?"v20_8080":"v20";
                return "v20";
        }
        throw std::runtime_error("unknown CPU architecture");
    }
    std::uint32_t disPc() const {
        if(selected_=="sub") {
            auto lock=m_.lockDisplay();
            return m_.systemBoard().displayCpu().regs.r.w.PC;
        }
        const auto* b=board(selected_);
        if(!b)throw std::runtime_error("CPU not installed");
        return b->cpu().pc();
    }
    std::uint32_t disLimit() const {
        return selected_=="main8088"?0x100000u:0x10000u;
    }
    std::string externalDisassemble(const std::string& arch,
                                    std::uint32_t pc,unsigned count) const {
        // CPU memory inspection only. MAME unidasm does all decoding.
        const auto boundary=disLimit();
        // MAME's largest supported instruction is no longer than 16 bytes.
        // Supply enough bytes for COUNT instructions without reading a
        // fixed 2 KiB window even for a single instruction.
        const auto window=static_cast<std::size_t>(count)*16;
        std::string bytes;
        bytes.reserve(window*2);
        {
            auto guard=m_.lockDisplay();
            for(std::size_t i=0;i<window;++i)
                bytes+=hex(peek((pc+i)%boundary,false),2);
        }
#ifndef FM11_DISASM_SCRIPT
        return "External disassembler not configured (FM11_DISASM_SCRIPT).\n";
#else
        // Safely quote the compile-time path, including literal apostrophes.
        const std::string script=FM11_DISASM_SCRIPT;
        std::string quoted="'";
        for(const char ch:script) {
            if(ch==39)quoted+=std::string(1,39)+"\\'"+std::string(1,39);
            else quoted+=ch;
        }
        quoted+="'";
        const auto command="python3 "+quoted+" "+arch+" "+hex(pc,boundary>65536?5:4)+
                           " "+std::to_string(count)+" "+bytes+" 2>&1";
        FILE* pipe=popen(command.c_str(),"r");
        if(!pipe)return "Cannot launch external disassembler.\n";
        std::string output;
        char buffer[1024];
        while(std::fgets(buffer,sizeof(buffer),pipe))
            output+=buffer;
        const int status=pclose(pipe);
        if(status!=0)return "External disassembler failed:\n"+output;
        return output.empty()?"No instructions decoded.\n":output;
#endif
    }
    BYTE peek(std::uint64_t addr,bool physical) const {
        if(physical) {
            for(std::size_t i=0;i<m_.boardCount();++i) {
                auto& b=m_.board(i);
                if(auto* p=dynamic_cast<Main6809Board*>(&b))return p->readPhysical(addr);
                if(auto* p=dynamic_cast<Main8088Board*>(&b))return p->bus().readRam(addr);
                if(auto* p=dynamic_cast<MainZ80Board*>(&b))return (*p->physicalMemory())[addr];
            }
            throw std::runtime_error("no main memory");
        }
        if(selected_=="sub")return m_.displaySubsystem().peekMemory(static_cast<WORD>(addr));
        auto* b=board(selected_);
        if(!b)throw std::runtime_error("CPU not installed");
        if(auto* p=dynamic_cast<Main6809Board*>(b))return p->peekCpu(static_cast<WORD>(addr));
        if(auto* p=dynamic_cast<Main8088Board*>(b))return p->bus().readMemory(addr);
        if(auto* p=dynamic_cast<MainZ80Board*>(b))return p->peekMemory(static_cast<WORD>(addr));
        throw std::runtime_error("unsupported CPU");
    }
    static constexpr std::size_t index(Target t) {return static_cast<std::size_t>(t);}
    static constexpr std::uint32_t limit(Target t) {
        return t==Target::Main8088?0x100000u:0x10000u;
    }
    static std::uint64_t encode(Target t,std::uint32_t pc) {
        return (std::uint64_t(index(t)+1)<<20)|pc;
    }
    static Target decodeCpu(std::uint64_t k) {
        return static_cast<Target>(((k>>20)&7)-1);
    }
    static std::uint32_t decodePc(std::uint64_t k) {
        return static_cast<std::uint32_t>(k&0xfffffu);
    }
    static std::string targetName(Target t) {
        switch(t) {
            case Target::Main6809:return "main6809";
            case Target::Main8088:return "main8088";
            case Target::Z80:return "z80";
            case Target::Sub:return "sub";
        }
        return "?";
    }
    static Target parseTarget(const std::string& s) {
        if(s=="main6809")return Target::Main6809;
        if(s=="main8088")return Target::Main8088;
        if(s=="z80")return Target::Z80;
        if(s=="sub")return Target::Sub;
        throw std::invalid_argument("unknown CPU target");
    }
    // 1 MiB / 64 bits for main8088, 64 KiB / 64 bits for the others.
    static constexpr std::array<std::size_t,4> wordCounts_{{1024,16384,1024,1024}};
    std::array<std::unique_ptr<std::atomic<std::uint64_t>[]>,4> bitmaps_;
    std::array<std::unique_ptr<std::atomic<std::uint64_t>[]>,4> watchReads_;
    std::array<std::unique_ptr<std::atomic<std::uint64_t>[]>,4> watchWrites_;
    std::set<std::tuple<std::string,char,std::uint32_t>> watches_;
    std::atomic<bool> hasWatches_{false};
    std::mutex breakMutex_; // monitor command thread only; never on hot path
    std::set<std::pair<std::string,std::uint32_t>> breaks_;
    std::size_t breakCount_=0;
    std::atomic<std::uint64_t> hit_{0};
    std::atomic<std::uint64_t> skip_{0};
    std::uint64_t stoppedAt_=0; // UI thread only
    std::atomic<bool> hasBreaks_{false};
    Machine& m_;
    PauseFn pause_;
    std::string selected_;
    std::ofstream log_;
    std::string file_;
};
