#pragma once
#include "portable_types.h"
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <utility>
#include <stdexcept>
#include <vector>

// System-side floppy media and sector storage, independent of the CPU/DMAC.
// The CPU boards retain only their FDC register and DMA-adapter behavior.
class FloppyDevice {
public:
    void setTimingTrace(std::function<void(const std::string&)> cb) { timingTrace_=std::move(cb); }
    void traceTiming(const std::string& msg) const { if(timingTrace_)timingTrace_(msg); }
    // Opt-in virtual mechanical time for both FDCs and CPU adapters.
    // Type-II rotation/latency is modeled; per-byte DRQ timing remains future work.
    void enableSeekTiming(bool enable) {seekTimed_=enable;resetSeekTiming();}
    bool seekTimingEnabled() const {return seekTimed_;}
    void resetSeekTiming() {
        for(auto& bank:seek_)for(auto& s:bank)s=SeekState{};
        for(unsigned ch=0;ch<2;++ch)
            for(unsigned d=0;d<4;++d)
                rotationNs_[ch][d]=(revolutionNanoseconds(ch)/4)*d;
        typeII_.fill(TypeIIState{});
        typeIIResult_.fill(0);
        tracks_.fill(TrackWriteState{});
        force_.fill(ForceInterruptState{});
    }
    bool beginTypeI(unsigned channel,unsigned drive,BYTE command) {
        if(!seekTimed_ || channel>1 || drive>3 || (command&0x80))return false;
        const unsigned kind=command&0xe0;
        if(kind!=0 && kind!=0x20 && kind!=0x40 && kind!=0x60)return false;
        auto& s=seek_[channel][drive];
        const unsigned maxTrack=channel?76u:39u;
        unsigned target=s.head;
        if(kind==0)target=(command&0x10)?registerValue(channel?0x33:0x1b):0;
        else if(kind==0x20)target=s.inward?std::min(maxTrack,s.head+1):(s.head?s.head-1:0);
        else if(kind==0x40){s.inward=true;target=std::min(maxTrack,s.head+1);}
        else {s.inward=false;target=s.head?s.head-1:0;}
        target=std::min(maxTrack,target);
        const unsigned distance=s.head>target?s.head-target:target-s.head;
        constexpr std::array<unsigned,4> stepMs{{6,12,20,30}};
        s.target=target;
        s.updateTrack=(kind==0 || (command&0x10)!=0);
        s.pending=true;s.complete=false;
        s.remainingNs=(std::uint64_t(std::max(1u,distance))*stepMs[command&3]+15)*1000000ULL;
        if(channel==0){registers_[0x1c]&=0x01;registers_[0x1f]=0;}
        else {registers_[0x30]=0x01;registers_[0x37]=0;}
        return true;
    }
    // Stage 2-A/B2: virtual rotation and per-byte Type-II DRQ.
    static constexpr std::uint64_t revolutionNanoseconds(unsigned ch) {
        return ch==0?200000000ULL:166666667ULL;
    }
    bool indexPulse(unsigned ch,unsigned drive) const {
        return ch<2 && drive<4 && seekTimed_ &&
               (ch==0?has2d(drive):has2hd(drive)) &&
               rotationNs_[ch][drive]<2000000ULL;
    }
    bool beginTypeII(unsigned ch,unsigned drive,unsigned track,unsigned side,
                     unsigned sector,BYTE command) {
        if(!seekTimed_ || ch>1 || drive>3 ||
           ((command&0xf0)!=0x80 && (command&0xf0)!=0xa0 &&
            (command&0xf0)!=0xc0))return false;
        auto& t=typeII_[ch];
        t=TypeIIState{};
        typeIIResult_[ch]=0;
        t.pending=true;t.drive=drive;t.command=command;
        const bool address=(command&0xf0)==0xc0;
        const auto rev=revolutionNanoseconds(ch);
        const auto& disk=medium(ch,drive);
        const unsigned n=disk.sectorsPerTrack;
        // Select the next ID after the current spindle phase, not the
        // previous ID (which would incorrectly wait a whole revolution).
        const auto lead=rev/64;
        const auto shifted=(rotationNs_[ch][drive]+rev-lead)%rev;
        const unsigned chosen=address && n?
            unsigned((shifted*n+rev-1)/rev)%n+1:sector;
        t.addressSector=chosen;
        std::size_t offset=0;
        if(!sectorOffset(ch,track,side,chosen,drive,offset)) {
            t.remainingNs=5*rev; // Record Not Found after five revolutions
            traceTiming("begin ch="+std::to_string(ch)+" d="+std::to_string(drive)+
                " C="+std::to_string(track)+" H="+std::to_string(side)+
                " R="+std::to_string(sector)+" no sector/head match; RNF pending");
        } else {
            const auto count=std::uint64_t(disk.sectorsPerTrack);
            const auto idPhase=((std::uint64_t(chosen-1)*rev)/count+rev/64)%rev;
            const auto search=(idPhase+rev-rotationNs_[ch][drive])%rev;
            const auto bytes=address?6:sectorLength(ch,track,side,chosen,drive);
            // FM-11's 2HD C0/H0 is a 26x128-byte single-density
            // boot track (250 kbps). The remaining 2HD tracks are
            // 500 kbps MFM. The original 6809 ROM reads C0/H0 by PIO,
            // so 16us DRQ there would make an otherwise valid boot fail.
            const auto bitrate=(ch==1 && track==0 && side==0 &&
                                disk.sectorsPerTrack==26 &&
                                (address || bytes==128))?250000ULL:
                                (ch?500000ULL:250000ULL);
            const auto transfer=(std::uint64_t(bytes)*8*1000000000ULL+bitrate-1)/bitrate;
            t.remainingNs=search;
            t.transferNs=transfer;
            t.bytePeriodNs=1000000000ULL*8/bitrate;
            t.byteCount=bytes;
            traceTiming("begin ch="+std::to_string(ch)+" d="+std::to_string(drive)+
                " C="+std::to_string(track)+" H="+std::to_string(side)+
                " R="+std::to_string(sector)+" wait_ns="+std::to_string(search)+
                " bytes="+std::to_string(bytes)+
                " period_ns="+std::to_string(t.bytePeriodNs));
        }
        return true;
    }

    // Stage 2-B3: D0 aborts without IRQ, D8 interrupts immediately;
    // D4/D2/D1 interrupt on the next INDEX / ready-down / ready-up edge.
    bool forceInterrupt(unsigned ch,unsigned drive,BYTE command) {
        if(ch>1 || drive>3 || (command&0xf0)!=0xd0)return false;
        cancelTypeII(ch);
        cancelTrackWrite(ch);
        cancelSeek(ch,drive);
        finishTransfer();
        auto& q=force_[ch];
        q=ForceInterruptState{};
        q.drive=drive;
        q.mask=BYTE(command&0x07);
        q.armed=q.mask!=0;
        // Immediate INTRQ is published by the issuing CPU adapter.
        // Do not leave a second deferred event that could re-assert IRQ.
        q.irq=false;
        q.lastReady=ch?has2hd(drive):has2d(drive);
        return (command&0x08)!=0;
    }
    void cancelForceInterrupt(unsigned ch) {
        if(ch<2)force_[ch]=ForceInterruptState{};
    }
    bool forceInterruptPending(unsigned ch) const {
        return ch<2 && force_[ch].irq;
    }
    bool takeForceInterrupt(unsigned ch) {
        if(!forceInterruptPending(ch))return false;
        force_[ch].irq=false;
        return true;
    }
    // Fast mode can observe conditional interrupts on FDC status polling.
    // No mechanical delays or DMA pacing are introduced.
    bool pollFastForceInterrupt(unsigned ch) {
        if(seekTimed_ || ch>1)return false;
        auto& q=force_[ch];
        if(!q.armed)return false;
        const bool ready=ch?has2hd(q.drive):has2d(q.drive);
        const bool changed=ready!=q.lastReady;
        const bool readyDown=changed && !ready && (q.mask&0x02);
        const bool readyUp=changed && ready && (q.mask&0x01);
        const bool index=(q.mask&0x04) && ready && (++q.polls%256u)==0u;
        q.lastReady=ready;
        if(index || readyDown || readyUp) {
            q.armed=false;
            q.irq=true;
        }
        return q.irq;
    }
    // Stage 2-B2 timed WRITE TRACK: all DMA bytes span one revolution.
    // The medium is updated only after the complete stream arrives.
    bool beginTrackWrite(unsigned ch,unsigned drive,unsigned track,unsigned side) {
        if(!seekTimed_ || ch>1 || drive>3 || side>1 ||
           !(ch?has2hd(drive):has2d(drive)) ||
           seek_[ch][drive].pending || seek_[ch][drive].head!=track)return false;
        auto& t=tracks_[ch];
        t=TrackWriteState{};
        t.active=true;t.drive=drive;t.track=track;t.side=side;
        t.length=ch?10900u:6400u;
        t.bytes.reserve(t.length);
        t.ready=true; // first DRQ begins with the write data interval
        traceTiming("WRITE TRACK begin ch="+std::to_string(ch)+
            " C="+std::to_string(track)+" H="+std::to_string(side)+
            " bytes="+std::to_string(t.length));
        return true;
    }
    bool trackBusy(unsigned ch,unsigned drive) const {
        return ch<2 && tracks_[ch].active && tracks_[ch].drive==drive;
    }
    bool trackByteReady(unsigned ch) const {
        return ch<2 && tracks_[ch].active && tracks_[ch].ready &&
               tracks_[ch].elapsed<revolutionNanoseconds(ch);
    }
    bool trackDone(unsigned ch) const {
        return ch<2 && tracks_[ch].active &&
               tracks_[ch].elapsed>=revolutionNanoseconds(ch);
    }
    bool takeTrackByte(unsigned ch,BYTE byte) {
        if(!trackByteReady(ch))return false;
        auto& t=tracks_[ch];
        t.bytes.push_back(byte);
        t.ready=false;
        return true;
    }
    bool finishTrackWrite(unsigned ch) {
        if(!trackDone(ch))return false;
        auto& t=tracks_[ch];
        const bool complete=!t.lost && t.bytes.size()==t.length;
        traceTiming("WRITE TRACK complete ch="+std::to_string(ch)+
            " served="+std::to_string(t.bytes.size())+
            " lost="+std::to_string(t.lost));
        const bool result=complete && formatTrack(ch,t.track,t.side,t.drive,t.bytes);
        t=TrackWriteState{};
        return result;
    }
    void cancelTrackWrite(unsigned ch) {
        if(ch<2)tracks_[ch]=TrackWriteState{};
    }
    bool typeIIBusy(unsigned ch,unsigned drive) const {
        return ch<2 && drive<4 && typeII_[ch].pending &&
               typeII_[ch].drive==drive;
    }
    // Stage 2-B2: byte-level DRQ clock. The adapter opts in after
    // locating a sector and arming its DMA/PIO transfer. The existing
    // two-edge command model remains available for legacy adapters.
    void startTypeIIBytePacing(unsigned ch) {
        if(ch>1)return;
        auto& t=typeII_[ch];
        if(!t.pending || !t.transferring || !t.byteCount)return;
        t.bytePaced=true;
        // Transfer rate is selected from the actual physical track,
        // not merely the controller channel.
        if(!t.bytePeriodNs)return;
        t.byteReady=true; // first byte when the data field arrives
        const auto elapsed=t.transferNs-t.remainingNs;
        const auto slots=elapsed/t.bytePeriodNs;
        if(slots) {
            t.byteReady=true;
            t.lostData=true; // first DRQ was not serviced in time
        }
        t.byteSlots=slots;
    }
    bool typeIIBytePaced(unsigned ch) const {
        return ch<2 && typeII_[ch].pending && typeII_[ch].bytePaced;
    }
    bool typeIIByteReady(unsigned ch) const {
        return ch<2 && typeII_[ch].pending &&
               typeII_[ch].bytePaced && typeII_[ch].byteReady;
    }
    bool acceptTypeIIByte(unsigned ch) {
        if(!typeIIByteReady(ch))return false;
        auto& t=typeII_[ch];
        t.byteReady=false;
        ++t.byteServed;
        return true;
    }
    bool typeIILostData(unsigned ch) const {
        return ch<2 && typeII_[ch].lostData;
    }
    BYTE typeIIResult(unsigned ch) const {
        return ch<2?typeIIResult_[ch]:0x10;
    }
    bool typeIIReady() const {
        for(const auto& t:typeII_)if(t.pending && t.remainingNs==0)return true;
        return false;
    }
    // Time to the next ID, byte DRQ, or Type-II completion. Used by
    // the machine scheduler to service hardware DMA even when one CPU
    // instruction spans multiple disk byte times.
    std::uint64_t nextTypeIIEventNanoseconds() const {
        std::uint64_t next=UINT64_MAX;
        for(const auto& t:typeII_) {
            if(!t.pending)continue;
            next=std::min(next,t.remainingNs);
            if(t.transferring && t.bytePaced && t.bytePeriodNs &&
               t.byteSlots+1<t.byteCount) {
                const auto elapsed=t.transferNs-t.remainingNs;
                const auto boundary=(t.byteSlots+1)*t.bytePeriodNs;
                if(boundary>elapsed)
                    next=std::min(next,boundary-elapsed);
            }
        }
        for(unsigned ch=0;ch<2;++ch) {
            const auto& q=force_[ch];
            if(q.armed && (q.mask&0x04)) {
                const auto rev=revolutionNanoseconds(ch);
                next=std::min(next,rev-rotationNs_[ch][q.drive]);
            }
            const auto& t=tracks_[ch];
            if(!t.active)continue;
            const auto rev=revolutionNanoseconds(ch);
            next=std::min<std::uint64_t>(next,rev>t.elapsed?rev-t.elapsed:0);
            if(t.slots+1<t.length) {
                const auto boundary=((t.slots+1)*rev+t.length-1)/t.length;
                if(boundary>t.elapsed)
                    next=std::min(next,boundary-t.elapsed);
            }
        }
        return next;
    }
    bool typeIITransferring(unsigned ch,unsigned drive) const {
        return ch<2 && drive<4 && typeII_[ch].pending &&
               typeII_[ch].drive==drive && typeII_[ch].transferring;
    }
    // Returns one event: at sector arrival, start DMA/PIO; at the
    // end of the data interval, release BUSY and publish completion.
    // An invalid ID has transferNs=0 and finishes with RNF after 5 turns.
    bool takeTypeIIEvent(unsigned ch,BYTE& command,bool& completed) {
        if(ch>1 || !typeII_[ch].pending || typeII_[ch].remainingNs)return false;
        auto& t=typeII_[ch];
        command=t.command;
        // A missing ID has no data phase: it completes with RNF
        // immediately after the five-revolution search timeout.
        completed=t.transferring || !t.transferNs;
        traceTiming("event ch="+std::to_string(ch)+
            (completed?" complete":" sector-arrival")+
            " overrun_ns="+std::to_string(t.overrunNs)+
            " served="+std::to_string(t.byteServed)+
            " expected="+std::to_string(t.byteCount)+
            " lost="+std::to_string(t.lostData));
        if(completed) {
            typeIIResult_[ch]=!t.transferNs?0x10:
                (t.bytePaced && (t.lostData || t.byteServed!=t.byteCount))?0x04:0;
            t=TypeIIState{};
        } else {
            t.transferring=true;
            t.remainingNs=t.transferNs>t.overrunNs?t.transferNs-t.overrunNs:0;
            t.overrunNs=0;
        }
        return true;
    }
    void cancelTypeII(unsigned ch) {
        if(ch<2)typeII_[ch]=TypeIIState{};
    }
    void advanceSeekNanoseconds(std::uint64_t ns) {
        if(!seekTimed_ || !ns)return;
        for(unsigned ch=0;ch<2;++ch) {
            const auto rev=revolutionNanoseconds(ch);
            auto& q=force_[ch];
            if(q.armed) {
                const bool ready=ch?has2hd(q.drive):has2d(q.drive);
                const bool changed=ready!=q.lastReady;
                const bool index=(q.mask&0x04) &&
                    ns>=rev-rotationNs_[ch][q.drive];
                const bool readyDown=changed && !ready && (q.mask&0x02);
                const bool readyUp=changed && ready && (q.mask&0x01);
                if(index || readyDown || readyUp) {
                    q.irq=true;
                    q.armed=false;
                }
                q.lastReady=ready;
            }
            for(unsigned d=0;d<4;++d)
                rotationNs_[ch][d]=(rotationNs_[ch][d]+ns%rev)%rev;
            auto& t=typeII_[ch];
            if(t.pending) {
                if(t.transferring && t.bytePaced && t.bytePeriodNs) {
                    const auto oldSlots=t.byteSlots;
                    const auto elapsed=t.transferNs-t.remainingNs+
                        std::min(ns,t.remainingNs);
                    const auto slots=std::min<std::uint64_t>(
                        t.byteCount?t.byteCount-1:0,elapsed/t.bytePeriodNs);
                    if(slots>oldSlots) {
                        if(t.byteReady || slots>oldSlots+1)
                            t.lostData=true;
                        t.byteReady=true;
                        t.byteSlots=slots;
                    }
                }
                if(ns>=t.remainingNs) {
                    // Preserve elapsed time after ID arrival so a large
                    // Machine::advanceTimeNanoseconds() call can produce
                    // both events without requiring a second advance.
                    t.overrunNs+=ns-t.remainingNs;
                    t.remainingNs=0;
                } else t.remainingNs-=ns;
            }
        }
        for(unsigned ch=0;ch<2;++ch) {
            auto& t=tracks_[ch];
            if(!t.active)continue;
            const auto rev=revolutionNanoseconds(ch);
            t.elapsed=std::min(rev,t.elapsed+ns);
            const auto slots=std::min<std::uint64_t>(
                t.length-1,(t.elapsed*t.length)/rev);
            if(slots>t.slots) {
                if(t.ready || slots>t.slots+1)t.lost=true;
                t.ready=true;
                t.slots=slots;
            }
        }
        for(unsigned ch=0;ch<2;++ch)for(unsigned d=0;d<4;++d){
            auto& s=seek_[ch][d];
            if(!s.pending)continue;
            if(ns<s.remainingNs){s.remainingNs-=ns;continue;}
            s.pending=false;s.remainingNs=0;s.head=s.target;s.complete=true;
            if(s.updateTrack)registers_[ch?0x31:0x19]=BYTE(s.head);
            if(ch==0){registers_[0x1c]|=0x40;registers_[0x1f]=registers_[0x1c];}
            else {registers_[0x30]=0x00;registers_[0x37]=0x40;}
        }
    }
    void cancelSeek(unsigned ch,unsigned drive) {
        if(!seekTimed_ || ch>1 || drive>3)return;
        auto& s=seek_[ch][drive];s.pending=false;s.complete=false;
    }
    bool seekBusy(unsigned ch,unsigned drive) const {return seek_.at(ch).at(drive).pending;}
    bool seekComplete(unsigned ch,unsigned drive) const {return seek_.at(ch).at(drive).complete;}
    void ackSeek(unsigned ch,unsigned drive) {seek_.at(ch).at(drive).complete=false;}
    unsigned headTrack(unsigned ch,unsigned drive) const {return seek_.at(ch).at(drive).head;}
    BYTE typeIStatus(unsigned ch,unsigned drive) const {
        const auto& s=seek_.at(ch).at(drive);
        if(s.pending)return 0x01;
        return BYTE((s.head==0?0x04:0) |
              (indexPulse(ch,drive)?0x02:0) |
              ((ch==0?has2d(drive):has2hd(drive))?0:0x80));
    }
    // The controller drive-select bits address independent inserted media.
    void setMountedDrive(unsigned drive) {
        if(drive>3)throw std::invalid_argument("FDC drive must be 0..3");
        mountedDrive_=drive;
    }
    unsigned mountedDrive() const {return mountedDrive_;}
    bool present(unsigned drive) const {return drive<4 && !media2hd_[drive].raw.empty();}
    bool has2d(unsigned drive=0) const {return drive<4 && !media2d_[drive].raw.empty();}
    bool has2hd(unsigned drive=0) const {return drive<4 && !media2hd_[drive].raw.empty();}
    bool hasAny2d() const {
        for(const auto& disk:media2d_)if(!disk.raw.empty())return true;
        return false;
    }
    bool hasAny2hd() const {
        for(const auto& disk:media2hd_)if(!disk.raw.empty())return true;
        return false;
    }
    void load2d(const std::vector<BYTE>& raw,unsigned drive=0) {
        if(drive>3)throw std::invalid_argument("2D drive must be 0..3");
        if(raw.size()!=40u*2*16*256)
            throw std::invalid_argument("FM-11 2D raw image must be 327680 bytes");
        auto& disk=media2d_[drive];
        disk.raw=raw;
        disk.d88.clear();
        disk.sectorsPerTrack=16;
        disk.slotSize=256;
        disk.offsets.assign(40u*2*16,0);
        disk.lengths.assign(disk.offsets.size(),256);
        disk.dirty=false;
    }
    void load2hd(const std::vector<BYTE>& raw,unsigned drive=0) {
        if(drive>3)throw std::invalid_argument("2HD drive must be 0..3");
        const bool native=raw.size()==77u*2*26*256;
        if(!native && raw.size()!=77u*2*8*1024)
            throw std::invalid_argument("FM-11 2HD raw image must be 77x2x26x256 or 77x2x8x1024 bytes");
        auto& disk=media2hd_[drive];
        disk.raw=raw;
        disk.d88.clear();
        disk.sectorsPerTrack=native?26:8;
        disk.slotSize=native?256:1024;
        disk.offsets.assign(77u*2*disk.sectorsPerTrack,0);
        disk.lengths.assign(disk.offsets.size(),disk.slotSize);
        disk.dirty=false;
    }
    void loadD88(const std::vector<BYTE>& d88,unsigned drive=0) {
        loadD88Medium(d88,drive,false);
    }
    void loadD882hd(const std::vector<BYTE>& d88,unsigned drive=0) {
        loadD88Medium(d88,drive,true);
    }
    void loadD88Medium(const std::vector<BYTE>& d88,unsigned drive,bool is2hd) {
        if(drive>3)throw std::invalid_argument("D88 drive must be 0..3");
        // D88 header: 0x2B0 bytes, 164 little-endian track offsets.
        constexpr std::size_t headerSize=0x2b0;
        const unsigned tracks=is2hd?77:40;
        // FM-11 native 2HD uses 26x128-byte FM records on C0/H0,
        // and 26x256-byte MFM records on all remaining tracks.
        // Keep the older 8x1024-byte layout accepted for compatibility.
        const bool native2hd=is2hd && d88.size()>headerSize+16 &&
            d88[0x2b0+3]==0 && d88[0x2b0+2]==1;
        const unsigned sectorsPerTrack=is2hd?(native2hd?26:8):16;
        const unsigned sectorSize=is2hd?(native2hd?256:1024):256;
        // D88 defines 00h=2D, 10h=2DD, 20h=2HD.  FM-11's
        // 40x2x16x256 Mini-FDC geometry is also commonly tagged 10h.
        // Preserve the strict physical CHRN check below for both labels.
        if(d88.size()<headerSize)
            throw std::invalid_argument("D88 header is truncated");
        auto le16=[&](std::size_t offset)->unsigned {
            return unsigned(d88[offset])|(unsigned(d88[offset+1])<<8);
        };
        auto le32=[&](std::size_t offset)->std::uint32_t {
            return std::uint32_t(d88[offset])|
                   (std::uint32_t(d88[offset+1])<<8)|
                   (std::uint32_t(d88[offset+2])<<16)|
                   (std::uint32_t(d88[offset+3])<<24);
        };
        if(le32(0x1c)!=d88.size())
            throw std::invalid_argument("D88 disk size field does not match file");
        if(is2hd ? d88[0x1b]!=0x20 :
                    (d88[0x1b]!=0x00 && d88[0x1b]!=0x10))
            throw std::invalid_argument(is2hd?"D88 is not a 2HD disk image":
                                             "D88 is not a 2D/2DD disk image");
        std::vector<BYTE> raw(std::size_t(tracks)*2*sectorsPerTrack*sectorSize);
        std::vector<bool> seen(std::size_t(tracks)*2*sectorsPerTrack);
        std::vector<std::size_t> offsets(seen.size());
        std::vector<unsigned> lengths(seen.size(),sectorSize);
        for(unsigned trackIndex=0;trackIndex<tracks*2;++trackIndex) {
            const auto trackStart=le32(0x20+trackIndex*4);
            if(!trackStart)continue;
            if(trackStart<headerSize || trackStart>d88.size()-16)
                throw std::invalid_argument("D88 track offset out of range");
            std::size_t cursor=trackStart;
            const auto sectors=le16(cursor+4);
            if(sectors==0 || sectors>256)
                throw std::invalid_argument("D88 sector count is invalid");
            for(unsigned i=0;i<sectors;++i) {
                if(cursor>d88.size()-16)
                    throw std::invalid_argument("D88 sector header is truncated");
                const unsigned c=d88[cursor],side=d88[cursor+1];
                const unsigned sector=d88[cursor+2],n=d88[cursor+3];
                const unsigned count=le16(cursor+4),length=le16(cursor+14);
                const unsigned expectedSize=(native2hd && trackIndex==0)?128:sectorSize;
                const unsigned expectedN=expectedSize==128?0:expectedSize==256?1:3;
                if(count!=sectors || sectors!=sectorsPerTrack ||
                   c!=trackIndex/2 || side!=trackIndex%2 ||
                   sector<1 || sector>sectorsPerTrack ||
                   n!=expectedN || length!=expectedSize || d88[cursor+8]!=0)
                    throw std::invalid_argument(is2hd?
                        "D88 sector layout is not FM-11 2HD":
                        "D88 sector layout is not FM-11 2D");
                cursor+=16;
                if(length>d88.size()-cursor)
                    throw std::invalid_argument("D88 sector data is truncated");
                const auto idx=(std::size_t(c)*2+side)*sectorsPerTrack+(sector-1);
                if(seen[idx])throw std::invalid_argument("D88 has duplicate sector IDs");
                seen[idx]=true;
                offsets[idx]=cursor;
                lengths[idx]=length;
                std::copy_n(d88.begin()+cursor,length,raw.begin()+idx*sectorSize);
                cursor+=length;
            }
        }
        for(bool present:seen)
            if(!present)
                throw std::invalid_argument(is2hd?
                    "D88 is missing an FM-11 2HD sector":
                    "D88 is missing an FM-11 2D sector");
        if(is2hd)load2hd(raw,drive);
        else load2d(raw,drive);
        auto& disk=is2hd?media2hd_[drive]:media2d_[drive];
        disk.d88=d88;
        disk.offsets=std::move(offsets);
        disk.lengths=std::move(lengths);
    }

    // The original FM-11 CMainCpu::Fm11WrtTrack decodes WD17xx
    // WRITE TRACK bytes: zero sync, FE C/H/R/N F7 address field,
    // and FB/F8 data field ending in F7.  Unlike the original's
    // count-only parser, require a complete, correctly sized and
    // unique sector set before changing the image.
    using ImageWriter=std::function<bool(unsigned,unsigned,const std::vector<BYTE>&)>;
    void setImageWriter(ImageWriter writer) {imageWriter_=std::move(writer);}

    bool formatTrack(unsigned channel,unsigned track,unsigned side,unsigned drive,
                     const std::vector<BYTE>& stream) {
        if(channel>1 || drive>3 || side>1)return false;
        auto& disk=medium(channel,drive);
        if(disk.raw.empty() || track>=(channel==0?40u:77u))return false;
        const unsigned sectors=disk.sectorsPerTrack;
        const unsigned size=(channel==1 && disk.slotSize==256 && track==0 && side==0)
                           ?128:disk.slotSize;
        if(size!=128 && size!=256 && size!=1024)return false;
        const BYTE n=size==128?0:size==256?1:3;
        std::vector<std::vector<BYTE>> payloads(sectors);
        std::vector<bool> seen(sectors,false);
        unsigned found=0;
        for(std::size_t i=0;i+5<stream.size();) {
            // The controller's address-mark sequence uses FE as a
            // special byte after sync; no CRC bytes are present in
            // the CPU's format stream (F7 requests CRC generation).
            if(stream[i]!=0xfe) {++i;continue;}
            if(stream[i+1]!=track || stream[i+2]!=side ||
               stream[i+3]<1 || stream[i+3]>sectors || stream[i+4]!=n ||
               stream[i+5]!=0xf7) {++i;continue;}
            const unsigned id=stream[i+3]-1;
            if(seen[id])return false;
            std::size_t mark=i+6;
            // Seek the data mark, stopping before another address field.
            while(mark<stream.size() && stream[mark]!=0xfb &&
                  stream[mark]!=0xf8 && stream[mark]!=0xf9 &&
                  stream[mark]!=0xfa && stream[mark]!=0xfe)++mark;
            if(mark>=stream.size() || stream[mark]==0xfe ||
               mark+1+size>=stream.size() || stream[mark+1+size]!=0xf7)
                {++i;continue;}
            payloads[id].assign(stream.begin()+mark+1,stream.begin()+mark+1+size);
            seen[id]=true;
            ++found;
            i=mark+2+size;
        }
        if(found!=sectors)return false;
        auto newRaw=disk.raw;
        const std::size_t trackBase=(std::size_t(track)*2+side)*sectors;
        for(unsigned id=0;id<sectors;++id) {
            const std::size_t pos=(trackBase+id)*disk.slotSize;
            std::fill_n(newRaw.begin()+pos,disk.slotSize,BYTE(0xe5));
            std::copy(payloads[id].begin(),payloads[id].end(),newRaw.begin()+pos);
        }

        auto newLengths=disk.lengths;
        for(unsigned id=0;id<sectors;++id)newLengths[trackBase+id]=size;
        std::vector<BYTE> newD88;
        std::vector<std::size_t> newOffsets=disk.offsets;
        if(!disk.d88.empty()) {
            // Regenerate the whole container: reformatting may change
            // physical sector sizes and consequently all track offsets.
            newD88.assign(disk.d88.begin(),disk.d88.begin()+0x2b0);
            const unsigned tracks=channel==0?40:77;
            auto put16=[&](std::size_t off,unsigned val) {
                newD88[off]=BYTE(val);newD88[off+1]=BYTE(val>>8);
            };
            auto put32=[&](std::size_t off,std::uint32_t val) {
                for(unsigned k=0;k<4;++k)newD88[off+k]=BYTE(val>>(8*k));
            };
            for(unsigned t=0;t<tracks*2;++t) {
                put32(0x20+4*t,std::uint32_t(newD88.size()));
                for(unsigned id=0;id<sectors;++id) {
                    const std::size_t idx=std::size_t(t)*sectors+id;
                    const unsigned len=newLengths[idx];
                    const auto header=newD88.size();
                    newD88.resize(header+16,0);
                    newD88[header]=BYTE(t/2);
                    newD88[header+1]=BYTE(t&1);
                    newD88[header+2]=BYTE(id+1);
                    newD88[header+3]=BYTE(len==128?0:len==256?1:3);
                    newD88[header+6]=BYTE(channel==1 && t==0?0x40:0);
                    put16(header+4,sectors);
                    put16(header+14,len);
                    newOffsets[idx]=newD88.size();
                    const std::size_t pos=idx*disk.slotSize;
                    newD88.insert(newD88.end(),newRaw.begin()+pos,newRaw.begin()+pos+len);
                }
            }
            put32(0x1c,std::uint32_t(newD88.size()));
        }
        // Persist the complete (possibly resized) D88 before publishing
        // the format to the running machine. For RAW, size stays fixed.
        const auto& output=newD88.empty()?newRaw:newD88;
        if(imageWriter_ && !imageWriter_(channel,drive,output))return false;
        disk.raw=std::move(newRaw);
        disk.d88=std::move(newD88);
        disk.lengths=std::move(newLengths);
        disk.offsets=std::move(newOffsets);
        disk.dirty=true;
        return true;
    }

    bool sectorOffset(unsigned channel,unsigned track,unsigned side,unsigned sector,
                      unsigned drive,std::size_t& offset) const {
        if(drive>=4 || channel>1)return false;
        if(seekTimed_ && (seek_[channel][drive].pending ||
                          seek_[channel][drive].head!=track))return false;
        const auto& disk=medium(channel,drive);
        const unsigned tracks=channel==0?40:77;
        if(disk.raw.empty() || track>=tracks || sector<1 ||
           sector>disk.sectorsPerTrack)return false;
        offset=((track*2+(side&1))*disk.sectorsPerTrack+(sector-1))*disk.slotSize;
        return true;
    }
    std::size_t sectorLength(unsigned channel,unsigned track=0,unsigned side=0,
                             unsigned sector=1,unsigned drive=0) const {
        const auto& disk=medium(channel,drive);
        const std::size_t idx=(std::size_t(track)*2+(side&1))*disk.sectorsPerTrack+
                              (sector-1);
        return idx<disk.lengths.size()?disk.lengths[idx]:disk.slotSize;
    }
    BYTE read(unsigned channel,std::size_t offset,unsigned drive=0) const {
        return medium(channel,drive).raw.at(offset);
    }
    void write(unsigned channel,std::size_t offset,BYTE value,unsigned drive=0) {
        auto& disk=medium(channel,drive);
        disk.raw.at(offset)=value;
        disk.dirty=true;
        if(!disk.d88.empty()) {
            const auto idx=offset/disk.slotSize;
            const auto inSector=offset%disk.slotSize;
            if(inSector<disk.lengths.at(idx))
                disk.d88.at(disk.offsets.at(idx)+inSector)=value;
        }
    }
    // Callback persists one complete sector to the selected host image.
    // Both CPU boards, including their PIO and DMA paths, use it.
    using SectorWriter=std::function<bool(unsigned,unsigned,std::size_t,const BYTE*,std::size_t)>;
    void setSectorWriter(SectorWriter writer) {sectorWriter_=std::move(writer);}
    bool writeSector(unsigned channel,std::size_t offset,const BYTE* bytes,unsigned drive=0) {
        const auto& disk=medium(channel,drive);
        const auto length=disk.lengths.at(offset/disk.slotSize);
        if(sectorWriter_ && !sectorWriter_(channel,drive,offset,bytes,length)) {
            status_=0x10;
            return false;
        }
        for(std::size_t i=0;i<length;++i)write(channel,offset+i,bytes[i],drive);
        return true;
    }
    std::size_t hostImageOffset(unsigned channel,std::size_t rawOffset,unsigned drive=0) const {
        const auto& disk=medium(channel,drive);
        if(!disk.d88.empty())
            return disk.offsets.at(rawOffset/disk.slotSize)+(rawOffset%disk.slotSize);
        return rawOffset;
    }
    // FDC control/data registers are system-side, not CPU-card local.
    // Command and data ports are dispatched separately by DMA adapters.
    BYTE registerValue(unsigned reg) const {return registers_.at(reg);}
    void writeRegister(unsigned reg,BYTE value) {registers_.at(reg)=value;}
    void resetRegisters() {registers_.fill(0);resetSeekTiming();}
    enum class Command : BYTE { Other, ReadSector, WriteSector, ReadAddress };
    static Command decodeCommand(BYTE value) {
        switch(value&0xf0) {
        case 0x80: return Command::ReadSector;
        case 0xa0: return Command::WriteSector;
        case 0xc0: return Command::ReadAddress;
        default: return Command::Other;
        }
    }
    // Command decoding is independent of the processor and its DMA chip.
    // Reset/seek commands still need their own controller implementation.
    // Shared FDC transfer FIFO. CPU-specific DMA controllers only move bytes
    // through this interface; the sector buffer belongs to the system device.
    // READ ADDRESS returns the ID of the next passing sector.
    // RAW and normalized D88 have no saved ID CRC, so compute CRC16.
    bool beginReadAddress(unsigned channel,unsigned track,unsigned side,
                          unsigned drive,unsigned sector=0) {
        if(channel>1 || drive>3)return false;
        if(!sector)sector=seekTimed_?typeII_[channel].addressSector:1;
        transferChannel_=channel;transferDrive_=drive;
        transferWriting_=false;transferLength_=0;transferPosition_=0;
        std::size_t off=0;
        if(!sectorOffset(channel,track,side,sector,drive,off)) {
            status_=0x10;return false;
        }
        const auto len=sectorLength(channel,track,side,sector,drive);
        BYTE n=0;
        while(n<3 && (128u<<n)<len)++n;
        transferBuffer_[0]=BYTE(track);
        transferBuffer_[1]=BYTE(side);
        transferBuffer_[2]=BYTE(sector);
        transferBuffer_[3]=n;
        std::uint16_t crc=0xffff;
        const auto add=[&](BYTE c){
            crc^=std::uint16_t(c)<<8;
            for(unsigned j=0;j<8;++j)
                crc=std::uint16_t((crc<<1)^((crc&0x8000)?0x1021:0));
        };
        add(0xfe);
        for(unsigned j=0;j<4;++j)add(transferBuffer_[j]);
        transferBuffer_[4]=BYTE(crc>>8);transferBuffer_[5]=BYTE(crc);
        transferLength_=6;status_=0x02;
        writeRegister(channel?0x32:0x1a,BYTE(sector));
        return true;
    }
    bool beginSector(unsigned channel,unsigned track,unsigned side,unsigned sector,
                     unsigned drive,bool writing) {
        transferChannel_=channel;
        transferDrive_=drive;
        transferLength_=0;
        transferPosition_=transferLength_;
        transferWriting_=false;
        status_=0;
        if(!sectorOffset(channel,track,side,sector,drive,transferOffset_)) {
            status_=0x10;
            return false;
        }
        transferLength_=sectorLength(channel,track,side,sector,drive);
        transferPosition_=0;
        transferWriting_=writing;
        if(!writing)
            for(std::size_t i=0;i<transferLength_;++i)
                transferBuffer_[i]=read(channel,transferOffset_+i,drive);
        status_=0x02;
        return true;
    }
    BYTE transferRead() {
        if(transferWriting_ || transferPosition_>=transferLength_)return 0xff;
        const BYTE result=transferBuffer_[transferPosition_++];
        if(transferPosition_==transferLength_)status_=0;
        return result;
    }
    void transferWrite(BYTE value) {
        if(!transferWriting_ || transferPosition_>=transferLength_)return;
        transferBuffer_[transferPosition_++]=value;
        if(transferPosition_==transferLength_) {
            const bool written=writeSector(transferChannel_,transferOffset_,transferBuffer_.data(),transferDrive_);
            transferWriting_=false;
            status_=written?0:0x10;
        }
    }
    void finishTransfer() {
        transferPosition_=transferLength_;
        transferWriting_=false;
        status_=0;
    }
    void transferError(BYTE status=0x10) {status_=status;}
    BYTE transferStatus() const {return status_;}
    std::size_t transferPosition() const {return transferPosition_;}
    std::size_t transferLength() const {return transferLength_;}
    unsigned transferChannel() const {return transferChannel_;}
    const std::vector<BYTE>& raw2d(unsigned drive=0) const {return media2d_.at(drive).raw;}
    const std::vector<BYTE>& raw2hd(unsigned drive=0) const {return media2hd_.at(drive).raw;}
    std::vector<BYTE> image2d(unsigned drive=0) const {
        const auto& disk=media2d_.at(drive);
        return disk.d88.empty()?disk.raw:disk.d88;
    }
    std::vector<BYTE> image2hd(unsigned drive=0) const {
        const auto& disk=media2hd_.at(drive);
        return disk.d88.empty()?disk.raw:disk.d88;
    }
    bool dirty2d(unsigned drive=0) const {return media2d_.at(drive).dirty;}
    bool dirty2hd(unsigned drive=0) const {return media2hd_.at(drive).dirty;}
    void clearDirty2d(unsigned drive=0) {media2d_.at(drive).dirty=false;}
    void clearDirty2hd(unsigned drive=0) {media2hd_.at(drive).dirty=false;}
private:
    struct SeekState {
        unsigned head=0,target=0;
        std::uint64_t remainingNs=0;
        bool inward=true,updateTrack=false,pending=false,complete=false;
    };
    struct TypeIIState {
        bool pending=false;
        unsigned drive=0,addressSector=0;
        BYTE command=0;
        std::uint64_t remainingNs=0,transferNs=0,overrunNs=0;
        bool transferring=false;
        bool bytePaced=false,byteReady=false,lostData=false;
        std::uint64_t bytePeriodNs=0,byteCount=0,byteSlots=0,byteServed=0;
    };
    struct ForceInterruptState {
        unsigned drive=0;
        BYTE mask=0;
        bool armed=false,irq=false,lastReady=false;
        unsigned polls=0;
    };
    std::array<ForceInterruptState,2> force_{};
    struct TrackWriteState {
        bool active=false,ready=false,lost=false;
        unsigned drive=0,track=0,side=0;
        std::uint64_t elapsed=0,slots=0,length=0;
        std::vector<BYTE> bytes;
    };
    std::array<TrackWriteState,2> tracks_{};
    std::function<void(const std::string&)> timingTrace_;
    bool seekTimed_=false;
    std::array<std::array<SeekState,4>,2> seek_{};
    std::array<std::array<std::uint64_t,4>,2> rotationNs_{};
    std::array<TypeIIState,2> typeII_{};
    std::array<BYTE,2> typeIIResult_{};
    struct Disk {
        std::vector<BYTE> raw,d88;
        std::vector<std::size_t> offsets;
        std::vector<unsigned> lengths;
        unsigned sectorsPerTrack=16,slotSize=256;
        bool dirty=false;
    };
    Disk& medium(unsigned channel,unsigned drive) {
        return channel==0?media2d_.at(drive):media2hd_.at(drive);
    }
    const Disk& medium(unsigned channel,unsigned drive) const {
        return channel==0?media2d_.at(drive):media2hd_.at(drive);
    }
    SectorWriter sectorWriter_;
    ImageWriter imageWriter_;
    unsigned mountedDrive_=0;
    std::array<Disk,4> media2d_{},media2hd_{};
    std::array<BYTE,256> registers_{};
    std::array<BYTE,1024> transferBuffer_{};
    std::size_t transferOffset_=0,transferPosition_=256,transferLength_=256;
    unsigned transferChannel_=0,transferDrive_=0;
    bool transferWriting_=false;
    BYTE status_=0;
};
