#include "Main6809Board.h"
#include <memory>
#include <vector>

namespace {

bool identityWhenDisabled()
{
    Main6809Board board(CpuType::MC6809);
    board.writePhysical(0x2345, 0x11);
    return board.readCpu(0x2345) == 0x11;
}

bool mapsFourKilobytePages()
{
    Main6809Board board(CpuType::MC6809);
    board.writeCpu(0xfd82, 0x34); // logical page 2 -> physical page 34h
    board.writePhysical(0x34567, 0x5a);
    board.writeCpu(0xfd93, 0x80); // MMR enable

    if (board.readCpu(0x2567) != 0x5a)
        return false;

    board.writeCpu(0x2678, 0xa5);
    return board.readPhysical(0x34678) == 0xa5;
}

bool switchesMmrContexts()
{
    Main6809Board board(CpuType::MC6809);

    board.writeCpu(0xfd90, 0x00);
    board.writeCpu(0xfd82, 0x12);
    board.writeCpu(0xfd90, 0x01);
    board.writeCpu(0xfd82, 0x56);

    board.writePhysical(0x12345, 0x12);
    board.writePhysical(0x56345, 0x56);
    board.writeCpu(0xfd93, 0x80);

    board.writeCpu(0xfd90, 0x00);
    if (board.readCpu(0x2345) != 0x12)
        return false;

    board.writeCpu(0xfd90, 0x01);
    return board.readCpu(0x2345) == 0x56;
}

bool masksContextRegisters()
{
    Main6809Board board(CpuType::MC6809);
    board.writeCpu(0xfd90, 0xf5);
    board.writeCpu(0xfd91, 0xa7);
    board.writeCpu(0xfd92, 0xbc);
    board.writeCpu(0xfd93, 0xc0);

    return board.readCpu(0xfd90) == 0x05 &&
           board.readCpu(0xfd91) == 0x07 &&
           board.readCpu(0xfd92) == 0xbc &&
           board.readCpu(0xfd93) == 0xc0;
}

bool keepsFc00ThroughFfffFixed()
{
    Main6809Board board(CpuType::MC6809);
    board.writeCpu(0xfd8f, 0x77); // logical page F -> physical page 77h
    board.writePhysical(0x77123, 0x77);
    board.writePhysical(0x0fc10, 0x7c);
    board.writeCpu(0xfc10, 0xfc);
    board.writeCpu(0xfd93, 0x80);

    if (board.readCpu(0xf123) != 0x77)
        return false;
    return board.readCpu(0xfc10) == 0xfc && board.readPhysical(0x0fc10) == 0x7c;
}

bool resetDisablesAndClearsMmr()
{
    Main6809Board board(CpuType::MC6809);
    board.writeCpu(0xfd90, 0x03);
    board.writeCpu(0xfd82, 0x44);
    board.writeCpu(0xfd93, 0x80);

    Main6809Board::BootRomImage bootRom{};
    bootRom[0x0ffe] = 0x12;
    bootRom[0x0fff] = 0x34;
    board.loadBootRom(bootRom);
    board.reset();

    if (board.cpu().pc() != 0x1234)
        return false;
    if (board.readCpu(0xfd90) != 0x00 || board.readCpu(0xfd93) != 0x00)
        return false;
    return board.readCpu(0xfd82) == 0x00;
}

// A live MMR write must update the corresponding cached page immediately.
// Updating an inactive context must not disturb the active mapping.
bool updatesMappedPageWithoutReset()
{
    Main6809Board board(CpuType::MC6809);
    board.writeCpu(0xfd82,0x12);
    board.writeCpu(0xfd93,0x80);
    board.writeCpu(0x2123,0xa1);
    if(board.readPhysical(0x12123)!=0xa1)return false;
    board.writeCpu(0xfd82,0x34);
    board.writeCpu(0x2123,0xb2);
    if(board.readCpu(0x2123)!=0xb2 ||
       board.readPhysical(0x34123)!=0xb2 ||
       board.readPhysical(0x12123)!=0xa1)return false;
    board.writeCpu(0xfd90,1);
    board.writeCpu(0xfd82,0x56);
    board.writeCpu(0x2123,0xc3);
    if(board.readPhysical(0x56123)!=0xc3)return false;
    board.writeCpu(0xfd90,0);
    return board.readCpu(0x2123)==0xb2;
}

bool togglesMmrAndRetainsFixedRegions()
{
    Main6809Board board(CpuType::MC6809);
    board.writePhysical(0x11234,0x71);
    board.writePhysical(0x0234,0x22);
    board.writeCpu(0xfd80,0x11);
    board.writeCpu(0xfd93,0x80);
    if(board.readCpu(0x0234)!=0x71)return false;
    board.writeCpu(0xfd93,0x00);
    if(board.readCpu(0x0234)!=0x22)return false;
    board.writeCpu(0xfd93,0x80);
    if(board.readCpu(0x0234)!=0x71)return false;
    board.writeCpu(0xfc00,0x55);
    return board.readCpu(0xfc00)==0x55;
}

bool windowOverridesCachedRamPage()
{
    Main6809Board board(CpuType::MC6809);
    board.writeCpu(0xfd87,0x12); // mapped page 7
    board.writeCpu(0xfd93,0x80);
    board.writePhysical(0x12c00,0x12);
    board.writePhysical(0x07c00,0x07);
    if(board.readCpu(0x7c00)!=0x12)return false;
    board.writeCpu(0xfd91,0x02);
    board.writeCpu(0xfd92,0x00);
    board.writeCpu(0xfd93,0xc0);
    board.writePhysical(0x27c00,0x27);
    if(board.readCpu(0x7c00)!=0x27)return false;
    board.writeCpu(0x7c00,0x44);
    if(board.readPhysical(0x27c00)!=0x44 ||
       board.readPhysical(0x12c00)!=0x12)return false;
    board.writeCpu(0xfd93,0x80);
    return board.readCpu(0x7c00)==0x12;
}

bool sharedBackingReplacementRefreshesPointers()
{
    Main6809Board board(CpuType::HD6309);
    board.writeCpu(0xfd83,0x5a);
    board.writeCpu(0xfd93,0x80);
    auto original=board.physicalMemory();
    auto replacement=std::make_shared<std::vector<BYTE>>(1u<<20,0);
    (*replacement)[0x5a123]=0x66;
    board.setPhysicalMemory(replacement);
    if(board.readCpu(0x3123)!=0x66)return false;
    board.writeCpu(0x3123,0x77);
    if((*replacement)[0x5a123]!=0x77 ||
       (*original)[0x5a123]==0x77)return false;
    board.reset();
    board.writeCpu(0x3123,0x88);
    return (*replacement)[0x3123]==0x88;
}

} // namespace

int main()
{
    if (!identityWhenDisabled())
        return 1;
    if (!mapsFourKilobytePages())
        return 2;
    if (!switchesMmrContexts())
        return 3;
    if (!masksContextRegisters())
        return 4;
    if (!keepsFc00ThroughFfffFixed())
        return 5;
    if (!resetDisablesAndClearsMmr())
        return 6;
    if (!updatesMappedPageWithoutReset())
        return 7;
    if (!togglesMmrAndRetainsFixedRegions())
        return 8;
    if (!windowOverridesCachedRamPage())
        return 9;
    if (!sharedBackingReplacementRefreshesPointers())
        return 10;
    return 0;
}
