#include "HostImageSync.h"
#include <array>
#include <atomic>
#include <chrono>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <thread>
#include <unistd.h>
#include <vector>

int main() {
    char path[]="/tmp/fm11-host-sync-XXXXXX";
    const int fd=::mkstemp(path);
    if(fd<0)return 1;
    ::close(fd);
    int error=0;
    try {
        // Check pwrite visibility, dirty tracking, and a new write during
        // fdatasync. An in-flight write must remain pending for the next tick.
        std::shared_ptr<HostImageSync::Image> image;
        std::atomic<int> syncCalls{0};
        {
            HostImageSync sync(std::chrono::seconds(10),[&](int imageFd) {
                if(++syncCalls==1) {
                    const unsigned char additional=0xb2;
                    if(!image->write(1,&additional,1))
                        throw std::runtime_error("write during fdatasync");
                }
                return ::fdatasync(imageFd);
            });
            image=sync.open(path);
            const unsigned char first=0xa1;
            if(!image->write(0,&first,1) || !image->dirty())return 2;
            unsigned char out=0;
            if(::pread(image->fd,&out,1,0)!=1 || out!=first)return 3;
            if(!sync.syncPending() || !image->dirty())return 4;
            if(!sync.syncPending() || image->dirty() || syncCalls!=2)return 5;
            std::vector<unsigned char> replacement{0x11,0x22,0x33};
            if(!image->replace(replacement) || !image->dirty())return 6;
            if(!sync.shutdown() || image->dirty())return 7;
            if(::pread(image->fd,&out,1,2)!=1 || out!=0x33)return 8;
            if(::lseek(image->fd,0,SEEK_END)!=3)return 9;
        }
        image.reset();

        // Failed fdatasync must NOT clear dirty and must be retried.
        {
            unsigned attempts=0;
            HostImageSync sync(std::chrono::seconds(10),[&](int f) {
                if(++attempts==1) {errno=EIO;return -1;}
                return ::fdatasync(f);
            });
            auto test=sync.open(path);
            const unsigned char one=0x77;
            if(!test->write(0,&one,1))return 10;
            if(sync.syncPending() || !test->dirty())return 11;
            if(!sync.syncPending() || test->dirty() || attempts!=2)return 12;
            if(!sync.shutdown())return 13;
        }

        // Periodic worker should sync without being explicitly asked.
        {
            std::atomic<unsigned> callbacks{0};
            HostImageSync sync(std::chrono::milliseconds(20),[&](int f) {
                ++callbacks;
                return ::fdatasync(f);
            });
            auto periodic=sync.open(path);
            const unsigned char value=0x88;
            if(!periodic->write(0,&value,1))return 14;
            for(int i=0;i<100 && periodic->dirty();++i)
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
            if(periodic->dirty() || callbacks.load()==0)return 15;
            // Final shutdown flushes writes even before the next 20ms tick.
            if(!periodic->write(0,&value,1))return 16;
            if(!sync.shutdown() || periodic->dirty())return 17;
        }
    } catch(const std::exception& e) {
        std::cerr<<e.what()<<"\n";
        error=18;
    }
    ::unlink(path);
    if(error)return error;
    std::cout<<"PASS: immediate pwrite, generation race, retry, periodic and exit sync\n";
    return 0;
}
