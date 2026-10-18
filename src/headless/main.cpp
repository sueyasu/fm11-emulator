#include "Main6809Board.h"

#include <array>
#include <cerrno>
#include <chrono>
#include <csignal>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <poll.h>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <termios.h>
#include <unistd.h>
#include <vector>

// This is a headless CPU/serial runner, not yet a complete FM-11 IPL.
namespace {
volatile std::sig_atomic_t stopping = 0;
void requestStop(int) { stopping = 1; }

class Terminal {
public:
    Terminal() {
        if (!isatty(STDIN_FILENO)) return;
        if (tcgetattr(STDIN_FILENO, &saved_) != 0) return;
        termios raw = saved_;
        raw.c_lflag &= static_cast<tcflag_t>(~(ICANON | ECHO));
        raw.c_iflag &= static_cast<tcflag_t>(~(ICRNL | IXON));
        raw.c_cc[VMIN] = 0;
        raw.c_cc[VTIME] = 0;
        if (tcsetattr(STDIN_FILENO, TCSANOW, &raw) == 0)
            active_ = true;
    }
    ~Terminal() {
        if (active_) tcsetattr(STDIN_FILENO, TCSANOW, &saved_);
    }
private:
    termios saved_{};
    bool active_ = false;
};

unsigned long long number(const std::string& value) {
    std::size_t end = 0;
    const auto n = std::stoull(value, &end, 0);
    if (end != value.size()) throw std::invalid_argument("invalid numeric argument");
    return n;
}

void loadBinary(Main6809Board& board, const std::string& file,
                unsigned long long origin, unsigned long long maxBootSize = 0) {
    if (origin > 0xffff) throw std::invalid_argument("load address out of range");
    std::ifstream in(file, std::ios::binary | std::ios::ate);
    if (!in) throw std::runtime_error("cannot open: " + file);
    const auto count = in.tellg();
    if (count < 0 || static_cast<unsigned long long>(count) > 0x10000 - origin)
        throw std::runtime_error("binary exceeds 64 KiB CPU address space");
    if (maxBootSize && (count == 0 || static_cast<unsigned long long>(count) > maxBootSize))
        throw std::runtime_error("boottrack size exceeds selected OS level or is empty");
    in.seekg(0);
    std::vector<char> bytes(static_cast<std::size_t>(count));
    if (!in.read(bytes.data(), count))
        throw std::runtime_error("cannot read: " + file);
    for (std::size_t i = 0; i < bytes.size(); ++i)
        board.writeCpu(static_cast<WORD>(origin + i),
                       static_cast<BYTE>(bytes[i]));
    // The IPL reads entire sectors. The disk-image builder zero-pads the
    // final sector when the generated boottrack is shorter than that span.
    for (std::size_t i = bytes.size(); maxBootSize && i < maxBootSize; ++i)
        board.writeCpu(static_cast<WORD>(origin + i), 0);
}

void usage() {
    std::cerr << "usage: fm11-headless (--binary FILE --load ADDR --pc ADDR"
                 " | --boottrack FILE --level 1|2)"
                 " [--floppy RAW-2D|D88] [--cpu 6809|6309] [--clock HZ] [--instructions COUNT]\n"
                 "  Boottrack starts AFTER the FM-11 floppy IPL has loaded it;\n"
                 "  there is no disk controller or subsequent disk I/O yet.\n"
                 "  Monitor on stdin; guest serial is on a separate /dev/pts/N.\n\n";
}
}

int main(int argc, char** argv) {
    try {
        std::string file, boottrack, floppy, cpu = "6809";
        unsigned long long load = 0, pc = 0, hz = 2000000, limit = 0;
        bool hasLoad = false, hasPc = false;
        int level = 0;
        for (int i=1;i<argc;++i) {
            const std::string a=argv[i];
            if (a=="--help") { usage(); return 0; }
            if (i+1>=argc) { usage(); return 2; }
            const std::string v=argv[++i];
            if (a=="--binary") file=v;
            else if (a=="--boottrack") boottrack=v;
            else if (a=="--floppy") floppy=v;
            else if (a=="--level") {
                const auto n=number(v);
                if(n!=1 && n!=2) throw std::invalid_argument("level must be 1 or 2");
                level=static_cast<int>(n);
            }
            else if (a=="--cpu") cpu=v;
            else if (a=="--load") {load=number(v);hasLoad=true;}
            else if (a=="--pc") {pc=number(v);hasPc=true;}
            else if (a=="--clock") hz=number(v);
            else if (a=="--instructions") limit=number(v);
            else {usage(); return 2;}
        }
        const bool bootMode = !boottrack.empty();
        if ((bootMode && (!file.empty() || hasLoad || hasPc || level==0)) ||
            (!bootMode && (file.empty() || !hasLoad || !hasPc || level!=0)) ||
            pc>0xffff || hz==0 || (cpu!="6809" && cpu!="6309")) {usage();return 2;}
        // FM-11 floppy IPL copies 17 (L1) or 21 (L2) 256-byte sectors
        // directly to these addresses, then jumps two bytes past the header.
        const unsigned long long bootBase = level==1 ? 0xeb00 : 0xe100;
        const unsigned long long bootSize = level==1 ? 17*256 : 21*256;
        if (bootMode) {load=bootBase; pc=bootBase+2;}
        Main6809Board board(cpu=="6809" ? CpuType::MC6809 : CpuType::HD6309);
        Main6809Board::BootRomImage rom{};
        rom[0xffe]=static_cast<BYTE>(pc>>8);
        rom[0xfff]=static_cast<BYTE>(pc);
        board.loadBootRom(rom);
        board.reset();
        board.setCpuClockHz(hz);
        if (!floppy.empty()) {
            std::ifstream disk(floppy,std::ios::binary|std::ios::ate);
            if(!disk)throw std::runtime_error("cannot open floppy: "+floppy);
            const auto size=disk.tellg();
            if(size<0 || size>4*1024*1024)
                throw std::runtime_error("unsupported floppy image size");
            disk.seekg(0);
            std::vector<BYTE> data(static_cast<std::size_t>(size));
            if(!disk.read(reinterpret_cast<char*>(data.data()),data.size()))
                throw std::runtime_error("cannot read floppy: "+floppy);
            if(data.size()==327680)board.loadFloppy2d(data);
            else board.loadFloppyD88(data);
        }
        loadBinary(board,bootMode ? boottrack : file,load,
                   bootMode ? bootSize : 0);
        // The original terminal is the monitor. The guest UART owns a PTY.
        const int master=posix_openpt(O_RDWR|O_NOCTTY|O_NONBLOCK);
        if(master<0 || grantpt(master)!=0 || unlockpt(master)!=0)
            throw std::runtime_error("cannot create serial PTY");
        const char* slave=ptsname(master);
        if(!slave)throw std::runtime_error("cannot resolve serial PTY");
        const std::string slavePath=slave;
        // Configure the slave to raw mode even before a terminal connects.
        const int slaveFd=open(slavePath.c_str(),O_RDWR|O_NOCTTY);
        if(slaveFd<0)throw std::runtime_error("cannot open serial slave");
        termios uart{};
        if(tcgetattr(slaveFd,&uart)==0) {
            cfmakeraw(&uart);
            tcsetattr(slaveFd,TCSANOW,&uart);
        }
        // Keep the slave descriptor open so an unattached terminal cannot
        // cause POLLHUP to spin the emulator's poll loop.
        std::signal(SIGINT,requestStop);
        std::signal(SIGTERM,requestStop);
        std::cout << "FM-11 headless monitor\nSerial: " << slavePath
                  << "\nCPU halted. Type help for commands.\n" << std::flush;
        bool running=false, quitting=false, inputOpen=true;
        unsigned long long count=0;
        // Emulated cycles advance the PTM inside executeOne(). Pace those
        // cycles against a monotonic host clock, rather than spin at full
        // speed while the guest waits for an interrupt or console input.
        using Clock=std::chrono::steady_clock;
        auto epoch=Clock::now();
        std::uint64_t pacedCycles=0;
        auto restartPacing=[&]() { epoch=Clock::now(); pacedCycles=0; };
        std::string line;
        std::vector<BYTE> pendingTx;
        auto hex=[](unsigned v,int width) {
            std::ostringstream o;
            o << std::uppercase << std::hex << std::setw(width)
              << std::setfill('0') << v;
            return o.str();
        };
        auto showRegs=[&]() {
            const auto& r=dynamic_cast<CSim09&>(board.cpu()).regs;
            std::cout << "PC=" << hex(r.r.w.PC,4)
                      << " S=" << hex(r.r.w.S,4)
                      << " U=" << hex(r.r.w.U,4)
                      << " X=" << hex(r.r.w.X,4)
                      << " Y=" << hex(r.r.w.Y,4)
                      << " A=" << hex(r.r.b.A,2)
                      << " B=" << hex(r.r.b.B,2)
                      << " DP=" << hex(r.DP,2)
                      << " CC=" << hex(r.CC,2)
                      << " MD=" << hex(r.MD,2) << "\n";
        };
        auto prompt=[&]() { std::cout << "fm11> " << std::flush; };
        auto drain=[&]() {
            BYTE c;
            while(board.takeSerialOutput(c))pendingTx.push_back(c);
            if(!pendingTx.empty()) {
                const ssize_t n=write(master,pendingTx.data(),pendingTx.size());
                if(n>0)pendingTx.erase(pendingTx.begin(),pendingTx.begin()+n);
                else if(n<0 && errno!=EAGAIN && errno!=EWOULDBLOCK && errno!=EIO)
                    throw std::runtime_error("PTY write failed");
            }
        };
        auto resetGuest=[&]() {
            board.reset();
            board.setCpuClockHz(hz);
            loadBinary(board,bootMode ? boottrack : file,load,bootMode ? bootSize : 0);
            pendingTx.clear();
            count=0;
            restartPacing();
        };
        auto command=[&](const std::string& text) {
            std::istringstream input(text);
            std::string cmd,arg,nstr;
            input >> cmd;
            if(cmd=="help"||cmd=="h"||cmd=="?") {
                std::cout << "c/continue, stop, s/step, r/regs, m/mem ADDRESS [LEN], "
                             "mp ADDRESS [LEN], reset, q/quit, h/help\n";
            } else if(cmd=="c"||cmd=="continue") {
                restartPacing();
                running=true;
                std::cout << "CPU running (type stop to pause).\n";
            } else if(cmd=="stop") {
                running=false;std::cout << "CPU halted.\n";
            } else if(cmd=="q"||cmd=="quit") {
                quitting=true;
            } else if(cmd=="r"||cmd=="regs")showRegs();
            else if(cmd=="s"||cmd=="step") {
                if(running)std::cout << "Stop CPU first.\n";
                else {board.cpu().executeOne();++count;drain();showRegs();}
            } else if(cmd=="reset") {
                if(running)std::cout << "Stop CPU first.\n";
                else {resetGuest();std::cout << "CPU reset and image reloaded.\n";showRegs();}
            } else if(cmd=="m"||cmd=="mem"||cmd=="mp") {
                input >> arg >> nstr;
                try {
                    if(arg.empty())throw std::invalid_argument("address required");
                    // Monitor addresses are hexadecimal by default.
                    const auto addr=std::stoull(arg,nullptr,16);
                    const auto len=nstr.empty()?64:std::stoull(nstr,nullptr,0);
                    const auto max=(cmd=="mp")?0x100000ULL:0x10000ULL;
                    if(addr>=max || len>256 || len>max-addr)
                        throw std::out_of_range("memory range");
                    for(unsigned long long i=0;i<len;++i) {
                        if(i%16==0)std::cout << hex(unsigned(addr+i),cmd=="mp"?5:4) << ":";
                        const BYTE b=(cmd=="mp")
                            ?board.readPhysical(std::uint32_t(addr+i))
                            :board.readCpu(WORD(addr+i));
                        std::cout << " " << hex(b,2);
                        if(i%16==15 || i+1==len)std::cout << "\n";
                    }
                }catch(const std::exception&) {
                    std::cout << "usage: m/mp HEXADDR [LENGTH, up to 256]\n";
                }
            } else if(!cmd.empty())std::cout << "Unknown command. Type help.\n";
            if(!running && !quitting)prompt();
        };
        prompt();
        while(!stopping && !quitting) {
            pollfd fds[2]={{inputOpen?STDIN_FILENO:-1,POLLIN,0},
                           {master,POLLIN,0}};
            // Run only when emulated time has not advanced beyond wall time.
            // A short positive poll timeout yields the host CPU when ahead,
            // while keeping monitor commands and PTY input responsive.
            int timeout=-1;
            if(running) {
                const auto elapsed=std::chrono::duration_cast<std::chrono::nanoseconds>(
                    Clock::now()-epoch).count();
                const auto due=static_cast<std::uint64_t>(
                    (static_cast<long double>(pacedCycles)*1000000000.0L)/hz);
                timeout=due>static_cast<std::uint64_t>(elapsed)?1:0;
            }
            const int ready=poll(fds,2,timeout);
            if(ready<0 && errno!=EINTR)throw std::runtime_error("poll failed");
            if(ready>0 && (fds[0].revents&(POLLIN|POLLHUP))) {
                char data[256];
                const ssize_t n=read(STDIN_FILENO,data,sizeof(data));
                if(n<=0) {
                    inputOpen=false;
                    if(!running)quitting=true;
                } else for(ssize_t i=0;i<n;++i) {
                    if(data[i]=='\n'||data[i]=='\r') {
                        const std::string one=line;line.clear();
                        command(one);
                    } else if(data[i]==0x7f || data[i]=='\b') {
                        if(!line.empty())line.pop_back();
                    } else if(line.size()<1024)line+=data[i];
                }
            }
            if(ready>0 && (fds[1].revents&POLLIN)) {
                BYTE data[256];
                const ssize_t n=read(master,data,sizeof(data));
                if(n>0)for(ssize_t i=0;i<n;++i)board.queueSerialInput(data[i]);
            }
            if(running) {
                for(unsigned i=0;i<256 && running && !stopping;++i) {
                    const auto elapsed=std::chrono::duration_cast<std::chrono::nanoseconds>(
                        Clock::now()-epoch).count();
                    const auto due=static_cast<std::uint64_t>(
                        (static_cast<long double>(pacedCycles)*1000000000.0L)/hz);
                    if(due>static_cast<std::uint64_t>(elapsed))break;
                    if(limit && count>=limit) {
                        running=false;std::cout << "Instruction limit reached.\n";
                        prompt();break;
                    }
                    board.cpu().executeOne();++count;
                    // All currently supported main CPUs expose instruction
                    // timing through Sim09. Unverified opcodes report zero;
                    // use a small nonzero fallback to avoid unbounded bursts.
                    const auto cycles=dynamic_cast<CSim09&>(board.cpu()).LastInstructionCycles();
                    pacedCycles+=cycles?cycles:4;
                }
            }
            drain();
        }
        close(slaveFd);
        close(master);
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "fm11-headless: " << ex.what() << '\n';
        return 1;
    }
}
