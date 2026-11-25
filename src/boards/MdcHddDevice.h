#pragma once
#include "portable_types.h"
#include <array>
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <utility>
#include <stdexcept>
#include <vector>

// FM-11 MDC hard-disk adapter (FDC0-FDC3), one physical drive.
// Uses the reference command protocol and 256-byte records.
// CPU-card DMA remains on the 6809/8088 adapter; the mounted medium
// and the command/status registers belong to the system board.
class MdcHddDevice {
public:
    struct Geometry {
        BYTE type;
        unsigned cylinders,heads,records;
    };
    using DmaTransfer=std::function<bool(unsigned,bool,std::vector<BYTE>&)>;
    using HostWrite=std::function<bool(std::size_t,const BYTE*,std::size_t)>;

    void load(const std::vector<BYTE>& bytes) {
        if(bytes.empty() || bytes.size()%256)
            throw std::invalid_argument("MDC disk image must contain 256-byte records");
        static constexpr std::array<Geometry,9> types{{
            {0x04,157,4,32},{0x08,315,2,32},{0x05,157,6,32},
            {0x09,315,4,32},{0x0a,315,6,32},{0x0c,315,8,32},
            {0x14,747,4,32},{0x15,747,7,32},{0x16,747,11,32}
        }};
        bool found=false;
        for(const auto& g:types) {
            if(bytes.size()<=std::size_t(g.cylinders)*g.heads*g.records*256) {
                geometry_=g;
                found=true;
                break;
            }
        }
        if(!found)throw std::invalid_argument("MDC disk image exceeds supported geometry");
        image_=bytes;
        modeBinary_=false;
        reset();
    }
    void enableTiming(bool enable) {timed_=enable; pending_=Pending{};}
    bool timingEnabled() const {return timed_;}
    bool busy() const {return phase_==3;}
    void advanceNanoseconds(std::uint64_t ns) {
        if(!timed_ || phase_!=3 || !ns)return;
        while(phase_==3 && ns>=pending_.remainingNs) {
            ns-=pending_.remainingNs;
            if(pending_.seekOnly) {
                currentCylinder_=pending_.cylinder;
                publishResult();
                break;
            }
            const std::size_t offset=pending_.start+pending_.completed;
            const std::size_t length=std::min<std::size_t>(
                256,pending_.length-pending_.completed);
            std::vector<BYTE> bytes(length);
            if(!pending_.writing)
                std::copy_n(image_.begin()+offset,length,bytes.begin());
            if(!dma_ || !dma_(pending_.dmaChannel,pending_.writing,bytes)) {
                failure(0x90,0x40,0x01,0x60);
                publishResult();
                break;
            }
            if(pending_.writing) {
                if(writer_ && !writer_(offset,bytes.data(),length)) {
                    failure(0x90,0x40,0x01,0x60);
                    publishResult();
                    break;
                }
                std::copy(bytes.begin(),bytes.end(),image_.begin()+offset);
            }
            pending_.completed+=length;
            if(pending_.completed==pending_.length) {
                const std::uint64_t next=pending_.record+
                    pending_.length/256;
                if(modeBinary_) {
                    result_[4]=BYTE(next>>24);result_[5]=BYTE(next>>16);
                    result_[6]=BYTE(next>>8);result_[7]=BYTE(next);
                } else {
                    auto n=next;
                    result_[7]=BYTE(n%geometry_.records);n/=geometry_.records;
                    result_[6]=BYTE(n%geometry_.heads);n/=geometry_.heads;
                    result_[5]=BYTE(n);n>>=8;result_[4]=BYTE(n);
                }
                result_[8]=result_[9]=0;
                currentCylinder_=pending_.cylinder;
                publishResult();
            } else pending_.remainingNs=recordTimeNs_;
        }
        if(phase_==3)pending_.remainingNs-=ns;
    }
    bool present() const {return !image_.empty();}
    // Parameter request is not an interrupt; result readiness is.
    bool irqPending() const {return phase_==2;}
    const Geometry& geometry() const {return geometry_;}
    void setDmaTransfer(DmaTransfer handler) {dma_=std::move(handler);}
    void setHostWriter(HostWrite handler) {writer_=std::move(handler);}
    void reset() {
        command_=0;
        phase_=0;
        selected_=0;
        params_.fill(0);
        result_.fill(0);
        paramPos_=resultPos_=paramExpected_=0;
        status_=0x80;
        control_=0x10;
        pending_=Pending{};
        currentCylinder_=0;
    }
    BYTE read(unsigned port) {
        switch(port&0xff) {
        case 0xc0:return status_;
        case 0xc1: {
            if(phase_!=2)return 0;
            const BYTE value=result_[resultPos_++];
            if(resultPos_>=result_.size()) {
                phase_=0;
                status_=0x80;
                control_=0x10;
            }
            return value;
        }
        case 0xc2:return selected_;
        case 0xc3:return control_;
        default:return 0xff;
        }
    }
    void write(unsigned port,BYTE value) {
        switch(port&0xff) {
        case 0xc0:
            command_=value;
            params_.fill(0);
            result_.fill(0);
            paramPos_=resultPos_=0;
            paramExpected_=parameterCount(command_);
            if(paramExpected_) {
                phase_=1;
                status_=0x21; // PREQ | DRQ
                control_=0x24;
            } else complete();
            break;
        case 0xc1:
            if(phase_==1 && paramPos_<params_.size()) {
                params_[paramPos_++]=value;
                if(paramPos_==paramExpected_)complete();
            }
            break;
        case 0xc2:
            selected_=value;
            if(value&0x08) {
                phase_=0;
                status_=0x80;
                control_=0x10;
                pending_=Pending{};
            }
            break;
        default:break;
        }
    }
    const std::vector<BYTE>& image() const {return image_;}
private:
    static unsigned parameterCount(BYTE cmd) {
        switch(cmd) {
        case 0x10:case 0x11:case 0x12:case 0x13:case 0x14:return 2;
        case 0x20:case 0x22:case 0x23:case 0x24:case 0x26:
        case 0x28:case 0x29:case 0x2a:case 0x2b:case 0x70:
        case 0x71:case 0x72:return 8;
        default:return 0;
        }
    }
    void success() {
        result_[0]=0x10;
        result_[1]=result_[2]=0;
        result_[3]=0x60;
    }
    void failure(BYTE first,BYTE second,BYTE third,BYTE fourth) {
        result_[0]=first;
        result_[1]=second;
        result_[2]=third;
        result_[3]=fourth;
    }
    void complete() {
        success();
        for(unsigned i=0;i<6;++i)result_[4+i]=params_[2+i];
        const unsigned drive=((selected_&3u)<<1)|(params_[0]&1u);
        if(command_==0x01 || command_==0x12 || command_==0x13) {
            result_[3]=0;
        } else if(command_==0x14) {
            if(drive!=0 || !present() || (params_[0]&0x10)) {
                failure(0x90,0x40,0x01,0);
                result_[8]=command_;
            } else {
                result_[3]=0;
                result_[4]=geometry_.type;
                result_[5]=0;
                result_[6]=2;
                result_[7]=BYTE(4|(modeBinary_?0x80:0));
                result_[8]=1;
                result_[9]=0;
            }
        } else if(command_==0x11) {
            if(drive!=0 || !present())failure(0x80,0x80,0x02,0);
            else if((params_[0]&0xf0)==0x60)modeBinary_=false;
            else if((params_[0]&0xf0)==0x70)modeBinary_=true;
        } else if(command_==0x10) {
            if(drive!=0 || !present())failure(0x80,0x80,0x02,0);
            else if(timed_)schedule(0,0,0,false,0,true);
        } else if(command_==0x20 || command_==0x22 ||
                  command_==0x23 || command_==0x24) {
            const std::uint64_t record=modeBinary_
                ?((std::uint64_t(params_[2])<<24) |
                  (std::uint64_t(params_[3])<<16) |
                  (std::uint64_t(params_[4])<<8) | params_[5])
                :(((std::uint64_t(params_[2])*256+params_[3]) *
                    geometry_.heads+params_[4])*geometry_.records+params_[5]);
            std::size_t length=(std::size_t(params_[6])<<8)|params_[7];
            if(length==0)length=geometry_.records*256;
            const std::uint64_t begin=record*256;
            const bool badChs=!modeBinary_ &&
                (params_[4]>=geometry_.heads || params_[5]>=geometry_.records);
            // Original MDC SEEK validates disk geometry (not image size)
            // and returns 90/40/10 for missing drives or invalid CHR/LBA.
            const std::uint64_t capacity=
                std::uint64_t(geometry_.cylinders)*geometry_.heads*
                geometry_.records;
            if(command_==0x20) {
                if(drive!=0 || !present())
                    failure(0x90,0x40,0x10,0);
                else if(badChs || record>=capacity)
                    failure(0x90,0x40,0x10,0x60);
                else if(timed_)
                    schedule(record,0,0,false,unsigned(record/
                        (geometry_.heads*geometry_.records)),true);
            } else if(drive!=0 || !present())
                failure(0x80,0x80,0x02,0);
            else if(badChs || begin>image_.size() ||
                    length>image_.size()-static_cast<std::size_t>(begin))
                failure(0x80,0x02,0x80,0x60);
            else {
                const bool writing=command_==0x23;
                if(timed_) {
                    schedule(record,static_cast<std::size_t>(begin),length,
                        writing,unsigned(record/
                            (geometry_.heads*geometry_.records)),false);
                } else {
                std::vector<BYTE> bytes(length);
                if(!writing) {
                    std::copy_n(image_.begin()+static_cast<std::size_t>(begin),
                                length,bytes.begin());
                }
                const unsigned dmaChannel=(selected_>>4)&3u;
                if(!dma_ || !dma_(dmaChannel,writing,bytes)) {
                    failure(0x90,0x40,0x01,0x60);
                } else if(writing) {
                    if(writer_ &&
                       !writer_(static_cast<std::size_t>(begin),bytes.data(),length))
                        failure(0x90,0x40,0x01,0x60);
                    else
                        std::copy(bytes.begin(),bytes.end(),
                                  image_.begin()+static_cast<std::size_t>(begin));
                }
                if(result_[0]==0x10) {
                    // Reference returns the advanced CHR/LBA and zero count.
                    const std::uint64_t next=record+length/256;
                    if(modeBinary_) {
                        result_[4]=BYTE(next>>24);
                        result_[5]=BYTE(next>>16);
                        result_[6]=BYTE(next>>8);
                        result_[7]=BYTE(next);
                    } else {
                        auto n=next;
                        result_[7]=BYTE(n%geometry_.records); n/=geometry_.records;
                        result_[6]=BYTE(n%geometry_.heads); n/=geometry_.heads;
                        result_[5]=BYTE(n); n>>=8;
                        result_[4]=BYTE(n);
                    }
                    result_[8]=result_[9]=0;
                }
                } // fast synchronous DMA
            }
        } else if(command_==0x71) {
            if(drive!=0 || !present())failure(0x80,0x80,0x02,0);
            else if(params_[2]!=geometry_.type || params_[5]!=2 ||
                    params_[6]!=1 || params_[7]!=0)
                failure(0x80,0x80,0x10,0);
        } else {
            failure(0x80,0x80,0x01,0);
            result_[8]=command_;
        }
        // The original controller echoes AD in result byte 0.
        if(paramExpected_)result_[0]=BYTE(result_[0]|(params_[0]&0x0f));
        if(phase_!=3)publishResult();
    }
    struct Pending {
        std::uint64_t record=0,remainingNs=0;
        std::size_t start=0,length=0,completed=0;
        unsigned dmaChannel=0,cylinder=0;
        bool writing=false,seekOnly=false;
    };
    void publishResult() {
        pending_=Pending{};
        phase_=2;
        resultPos_=0;
        status_=0x43;
        control_=0x4c;
    }
    void schedule(std::uint64_t record,std::size_t start,
                  std::size_t length,bool writing,unsigned cylinder,
                  bool seekOnly) {
        pending_=Pending{};
        pending_.record=record;pending_.start=start;
        pending_.length=length;pending_.writing=writing;
        pending_.cylinder=cylinder;pending_.seekOnly=seekOnly;
        pending_.dmaChannel=(selected_>>4)&3u;
        const unsigned steps=currentCylinder_>cylinder?
            currentCylinder_-cylinder:cylinder-currentCylinder_;
        pending_.remainingNs=(steps?2000000ULL+steps*200000ULL:0ULL)+
            (seekOnly?2000000ULL:8333333ULL+recordTimeNs_);
        phase_=3;status_=0x01;control_=0x00;
    }
    static constexpr std::uint64_t recordTimeNs_=1000000ULL;
    std::vector<BYTE> image_;
    Geometry geometry_{0x09,315,4,32};
    DmaTransfer dma_;
    HostWrite writer_;
    BYTE command_=0,phase_=0,selected_=0,status_=0x80,control_=0x10;
    unsigned paramPos_=0,resultPos_=0,paramExpected_=0;
    bool modeBinary_=false,timed_=false;
    unsigned currentCylinder_=0;
    Pending pending_{};
    std::array<BYTE,8> params_{};
    std::array<BYTE,10> result_{};
};
