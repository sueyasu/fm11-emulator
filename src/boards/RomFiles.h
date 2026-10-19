#pragma once
#include "portable_types.h"
#include <array>
#include <fstream>
#include <stdexcept>
#include <string>

// Firmware remains outside the source tree's version control. Loading is
// explicit: never silently substitute an empty ROM or truncate a dump.
namespace RomFiles {
template <std::size_t N>
std::array<BYTE,N> load(const std::string& path) {
    std::ifstream file(path,std::ios::binary|std::ios::ate);
    if(!file)throw std::runtime_error("cannot open ROM: "+path);
    const auto length=file.tellg();
    if(length!=static_cast<std::streamoff>(N))
        throw std::runtime_error("ROM has incorrect size: "+path+
                                 " (expected "+std::to_string(N)+" bytes)");
    std::array<BYTE,N> image{};
    file.seekg(0);
    if(!file.read(reinterpret_cast<char*>(image.data()),N))
        throw std::runtime_error("cannot read ROM: "+path);
    return image;
}
}
