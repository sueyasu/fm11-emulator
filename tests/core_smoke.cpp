#include "Sim09.h"
#include "SimZ80.h"

#include <array>
#include <cstdint>

int main()
{
    CSim09 cpu09;
    CSimZ80 z80;
    std::array<BYTE, 65536> memory{};
    z80.Memory = memory.data();

    z80.SetMem(0x1234, 0x5a);
    if (z80.GetMem(0x1234) != 0x5a)
        return 1;

    cpu09.SimRESET();
    return 0;
}
