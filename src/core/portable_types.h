#pragma once

#include <cstdint>
#include <cstring>

using BYTE = std::uint8_t;
using WORD = std::uint16_t;
using DWORD = std::uint32_t;
using LONG = std::int32_t;
using BOOL = int;

#ifndef TRUE
#define TRUE 1
#endif
#ifndef FALSE
#define FALSE 0
#endif

inline void ZeroMemory(void* ptr, std::size_t size)
{
    std::memset(ptr, 0, size);
}

static_assert(sizeof(BYTE) == 1);
static_assert(sizeof(WORD) == 2);
static_assert(sizeof(DWORD) == 4);
static_assert(sizeof(LONG) == 4);
