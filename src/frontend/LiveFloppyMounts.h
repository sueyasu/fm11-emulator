#pragma once
#include "FloppyDevice.h"
#include "HostImageSync.h"
#include <algorithm>
#include <array>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

// All methods (except the host image sync worker) run on the SDL thread
// with the guest CPU, display CPU and PTM paused. The FDC write callbacks
// may run on the guest CPU worker; they are only accessed by the SDL thread
// while that worker is stopped.
class LiveFloppyMounts : public std::enable_shared_from_this<LiveFloppyMounts> {
public:
    LiveFloppyMounts(std::shared_ptr<FloppyDevice> floppy,HostImageSync& sync)
        :floppy_(std::move(floppy)),sync_(sync) {
        if(!floppy_)throw std::invalid_argument("null floppy device");
    }
    // Install once; unlike the old capture-by-value array, the callback
    // reads the CURRENT mounted image for every guest write.
    void installWriters() {
        const std::weak_ptr<LiveFloppyMounts> owner=weak_from_this();
        floppy_->setSectorWriter([owner](unsigned ch,unsigned drive,
                                         std::size_t rawOffset,const BYTE* data,
                                         std::size_t length) {
            const auto self=owner.lock();
            if(!self || ch>1 || drive>3 || !self->images_[ch][drive])return false;
            const auto offset=self->floppy_->hostImageOffset(ch,rawOffset,drive);
            return self->images_[ch][drive]->write(offset,data,length);
        });
        floppy_->setImageWriter([owner](unsigned ch,unsigned drive,
                                        const std::vector<BYTE>& bytes) {
            const auto self=owner.lock();
            return self && ch<2 && drive<4 && self->images_[ch][drive] &&
                   self->images_[ch][drive]->replace(bytes);
        });
    }
    const std::string& path(unsigned ch,unsigned drive) const {
        return paths_.at(ch).at(drive);
    }
    // Called only when all guest workers have stopped; throw on a busy FDC,
    // duplicate physical file, malformed image or failed final disk sync.
    // No settings file is saved here.
    void exchange(unsigned ch,unsigned drive,const std::string& path) {
        if(ch>1 || drive>3)throw std::invalid_argument("invalid FDD channel/drive");
        if(!floppy_->canExchangeMedia(ch,drive))
            throw std::runtime_error("FDC operation is active; retry after I/O finishes");
        if(paths_[ch][drive]==path)return;
        std::shared_ptr<HostImageSync::Image> newImage;
        FloppyDevice candidate;
        if(!path.empty()) {
            // The same inode must not be mounted by multiple virtual drives:
            // different relative/absolute spellings may refer to one image.
            const auto normalized=std::filesystem::weakly_canonical(path);
            for(unsigned c=0;c<2;++c)for(unsigned d=0;d<4;++d) {
                if(c==ch && d==drive)continue;
                if(!paths_[c][d].empty() &&
                   std::filesystem::weakly_canonical(paths_[c][d])==normalized)
                    throw std::runtime_error("disk image already mounted in another FDD: "+path);
            }
            std::ifstream stream(path,std::ios::binary);
            if(!stream)throw std::runtime_error("cannot read floppy image: "+path);
            std::vector<BYTE> bytes(std::istreambuf_iterator<char>{stream},
                                    std::istreambuf_iterator<char>{});
            if(stream.bad())throw std::runtime_error("cannot finish reading floppy image: "+path);
            // Validate geometry completely without modifying the live FDC.
            if(ch==0) {
                if(bytes.size()==40u*2*16*256)candidate.load2d(bytes,drive);
                else candidate.loadD88(bytes,drive);
            } else {
                if(bytes.size()==77u*2*8*1024 ||
                   bytes.size()==77u*2*26*256)candidate.load2hd(bytes,drive);
                else candidate.loadD882hd(bytes,drive);
            }
            newImage=sync_.open(path);
        }
        std::string committedPath(path);
        const auto oldImage=images_[ch][drive];
        if(!sync_.syncImage(oldImage)) {
            // Do not leak the newly registered descriptor on failure.
            sync_.unregisterImage(newImage);
            throw std::runtime_error("old floppy image could not be synchronized");
        }
        // No throwing allocations after this point: swapping the prepared
        // image and callback slot is safe while guest workers are paused.
        floppy_->exchangeMedia(ch,drive,candidate);
        images_[ch][drive].swap(newImage);
        paths_[ch][drive].swap(committedPath);
        sync_.unregisterImage(oldImage);
    }
private:
    std::shared_ptr<FloppyDevice> floppy_;
    HostImageSync& sync_;
    std::array<std::array<std::shared_ptr<HostImageSync::Image>,4>,2> images_{};
    std::array<std::array<std::string,4>,2> paths_{};
};
