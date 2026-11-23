#pragma once
#include <array>
#include <cstdint>

// FM-11 AD2+ Communication DIP SW4..SW8, from the supplied DIPSW-2 table.
// Switch bit 4 is SW4 (x=1), bit 0 is SW8. Entries that the manual leaves
// undefined must not be inferred from the divider alone.
namespace Fm11SerialClock {
struct Setting { unsigned divider, asyncBaud, fastBaud; };
constexpr std::array<Setting,32> settings{{
    {256,300,1200}, {128,600,2400}, {64,1200,4800}, {32,2400,9600},
    {16,4800,19200}, {8,9600,0}, {0,0,0}, {0,0,0},
    {1024,75,300}, {512,150,600}, {256,0,0}, {128,0,0},
    {64,0,0}, {32,0,0}, {16,0,0}, {0,0,0},
    {4096,0,0}, {2048,0,0}, {1024,0,0}, {512,0,0},
    {256,0,0}, {128,0,0}, {64,0,0}, {0,0,0},
    {16384,0,0}, {8192,0,0}, {4096,0,0}, {2048,0,0},
    {1024,0,0}, {512,0,0}, {256,0,0}, {0,0,0}
}};
constexpr Setting lookup(unsigned sw4to8) {
    return sw4to8<settings.size()?settings[sw4to8]:Setting{0,0,0};
}
}
