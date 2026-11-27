#pragma once

// SDL host-image persistence. Guest writes update the mounted file with
// pwrite immediately; only fdatasync is deferred to a 100 ms worker tick.
// Each image owns its descriptor and a dirty generation. A write arriving
// during fdatasync is never incorrectly marked clean.
#include <atomic>
#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <functional>
#include <iostream>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <unistd.h>
#include <vector>

class HostImageSync {
public:
    struct Image {
        Image(int descriptor,std::string imageName)
            :fd(descriptor),name(std::move(imageName)) {}
        Image(const Image&)=delete;
        Image& operator=(const Image&)=delete;
        ~Image() {if(fd>=0)::close(fd);}

        // Serialize writes to a single image, including WRITE TRACK
        // replacement/truncation, without blocking on fdatasync.
        bool write(std::size_t offset,const unsigned char* bytes,std::size_t size) {
            std::lock_guard<std::mutex> lock(writeMutex);
            std::size_t done=0;
            while(done<size) {
                const auto n=::pwrite(fd,bytes+done,size-done,
                                      static_cast<off_t>(offset+done));
                if(n<0 && errno==EINTR)continue;
                if(n<=0) {
                    if(done)generation.fetch_add(1,std::memory_order_release);
                    std::cerr<<"host image write failed ("<<name<<"): "
                             <<(n<0?std::strerror(errno):"zero-byte write")<<"\n";
                    return false;
                }
                done+=static_cast<std::size_t>(n);
            }
            if(done)generation.fetch_add(1,std::memory_order_release);
            return true;
        }

        // A formatted D88 can change size; preserve write + truncate order.
        bool replace(const std::vector<unsigned char>& bytes) {
            std::lock_guard<std::mutex> lock(writeMutex);
            std::size_t done=0;
            while(done<bytes.size()) {
                const auto n=::pwrite(fd,bytes.data()+done,bytes.size()-done,
                                      static_cast<off_t>(done));
                if(n<0 && errno==EINTR)continue;
                if(n<=0) {
                    if(done)generation.fetch_add(1,std::memory_order_release);
                    std::cerr<<"host image replacement failed ("<<name<<"): "
                             <<(n<0?std::strerror(errno):"zero-byte write")<<"\n";
                    return false;
                }
                done+=static_cast<std::size_t>(n);
            }
            if(::ftruncate(fd,static_cast<off_t>(bytes.size()))!=0) {
                generation.fetch_add(1,std::memory_order_release);
                std::cerr<<"host image truncate failed ("<<name<<"): "
                         <<std::strerror(errno)<<"\n";
                return false;
            }
            generation.fetch_add(1,std::memory_order_release);
            return true;
        }

        bool dirty() const {
            return generation.load(std::memory_order_acquire)!=
                   synced.load(std::memory_order_acquire);
        }

        const int fd;
        const std::string name;

    private:
        friend class HostImageSync;
        std::mutex writeMutex;
        std::atomic<std::uint64_t> generation{0};
        std::atomic<std::uint64_t> synced{0};
    };

    using SyncFunction=std::function<int(int)>;
    explicit HostImageSync(
        std::chrono::milliseconds period=std::chrono::milliseconds(100),
        SyncFunction sync=[](int fd){return ::fdatasync(fd);})
        :period_(period),sync_(std::move(sync)),
         worker_([this]{run();}) {}

    HostImageSync(const HostImageSync&)=delete;
    HostImageSync& operator=(const HostImageSync&)=delete;

    // Open before registering so an image cannot be tracked without a
    // writable descriptor. The service owns and closes all registered FDs.
    std::shared_ptr<Image> open(const std::string& path) {
        const int fd=::open(path.c_str(),O_RDWR);
        if(fd<0)
            throw std::runtime_error("cannot open writable disk image: "+
                                     path+": "+std::strerror(errno));
        auto image=std::make_shared<Image>(fd,path);
        {
            std::lock_guard<std::mutex> lock(mutex_);
            images_.push_back(image);
        }
        return image;
    }

    // Stage 4-B: sync a specific media before detaching it. The image
    // remains mounted on failure; the caller must abort the exchange.
    bool syncImage(const std::shared_ptr<Image>& image) {
        if(!image)return true;
        // The sync service may be running concurrently. Image::writeMutex
        // serializes the final sync against writes to the same descriptor.
        std::lock_guard<std::mutex> lock(image->writeMutex);
        const auto target=image->generation.load(std::memory_order_acquire);
        if(target==image->synced.load(std::memory_order_acquire))return true;
        int result;
        do {result=sync_(image->fd);} while(result<0 && errno==EINTR);
        if(result!=0) {
            std::cerr<<"host image fdatasync failed ("<<image->name
                     <<"): "<<std::strerror(errno)<<"\n";
            return false;
        }
        image->synced.store(target,std::memory_order_release);
        return true;
    }
    // Drop the service's reference after a successful final sync.
    // A concurrent background sync retains its temporary shared_ptr,
    // so its descriptor remains valid until that sync finishes.
    void unregisterImage(const std::shared_ptr<Image>& image) {
        if(!image)return;
        std::lock_guard<std::mutex> lock(mutex_);
        images_.erase(std::remove(images_.begin(),images_.end(),image),
                      images_.end());
    }
    // Return false on any pending sync failure. Failed images remain dirty
    // and are retried on the next tick or final shutdown.
    bool syncPending() {
        std::vector<std::shared_ptr<Image>> images;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            images=images_;
        }
        bool okay=true;
        for(const auto& image:images) {
            const auto target=image->generation.load(std::memory_order_acquire);
            if(target==image->synced.load(std::memory_order_acquire))continue;
            int result;
            do {result=sync_(image->fd);} while(result<0 && errno==EINTR);
            if(result==0) {
                // Only the generation observed BEFORE fdatasync is
                // certified. Concurrent writes remain dirty.
                image->synced.store(target,std::memory_order_release);
            } else {
                const int error=errno;
                std::cerr<<"host image fdatasync failed ("<<image->name<<"): "
                         <<std::strerror(error)<<"\n";
                okay=false;
            }
        }
        return okay;
    }

    bool shutdown() {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if(!stopping_) {
                stopping_=true;
                wake_.notify_one();
            }
        }
        if(worker_.joinable())worker_.join();
        // Called only after emulated CPU workers have stopped writing.
        return syncPending();
    }

    ~HostImageSync() {if(!shutdown())std::cerr<<"host image final sync failed\n";}

private:
    void run() {
        std::unique_lock<std::mutex> lock(mutex_);
        while(!stopping_) {
            if(wake_.wait_for(lock,period_,[this]{return stopping_;}))
                break;
            lock.unlock();
            syncPending();
            lock.lock();
        }
    }

    const std::chrono::milliseconds period_;
    SyncFunction sync_;
    std::mutex mutex_;
    std::condition_variable wake_;
    bool stopping_=false;
    std::vector<std::shared_ptr<Image>> images_;
    std::thread worker_;
};
