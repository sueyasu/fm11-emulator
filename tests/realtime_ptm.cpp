#include "Machine.h"
#include "MainBoard.h"
#include <chrono>
#include <cstdint>
#include <thread>

namespace {
std::uint16_t counter(MainBoard& board) {
    BYTE hi=0,lo=0;
    if(!board.readIo(0xfd3a,hi) || !board.readIo(0xfd3b,lo))
        return 0;
    return static_cast<std::uint16_t>((unsigned(hi)<<8)|lo);
}
void latch(MainBoard& board,std::uint16_t count) {
    board.writeIo(0xfd3a,static_cast<BYTE>(count>>8));
    board.writeIo(0xfd3b,static_cast<BYTE>(count));
}
}
int main() {
    using namespace std::chrono_literals;
    MainBoard board;
    // Channel 0 begins at 20000 PTM ticks (over two seconds).
    latch(board,20000);
    board.startRealtimePtm();
    if(!board.realtimePtmEnabled())return 1;
    const auto before=counter(board);
    std::this_thread::sleep_for(30ms);
    board.pollRealtimePtm();
    const auto after=counter(board);
    if(before<=after || before-after<50 || before-after>2000)return 2;
    // Guest time acceleration must no longer drive the PTM.
    board.advanceCpuCycles(50000000,2000000);
    board.advanceTimeNanoseconds(50000000000ULL);
    const auto noDoubleAdvance=counter(board);
    if(after<noDoubleAdvance || after-noDoubleAdvance>1000)return 3;

    // Configure channel 1 to raise a periodic IRQ; the worker owns its
    // counters, while the CPU safely observes its published IRQ level.
    board.writeIo(0xfd3c,0x00);
    board.writeIo(0xfd3d,0x0a);
    board.writeIo(0xfd39,0x50);
    std::this_thread::sleep_for(10ms);
    board.pollRealtimePtm();
    if(!board.ptmIrqLevel() || (board.irqLines()&1)==0)return 4;
    BYTE status=0;
    board.readIo(0xfd39,status);
    if((status&2)==0)return 5;

    board.stopRealtimePtm();
    if(board.realtimePtmEnabled())return 6;
    // Legacy virtual time remains functional after the timer stops.
    board.reset();
    latch(board,20000);
    const auto legacyBefore=counter(board);
    board.advanceTimeNanoseconds(100000000ULL);
    const auto legacyAfter=counter(board);
    if(legacyBefore<=legacyAfter ||
       legacyBefore-legacyAfter<900 || legacyBefore-legacyAfter>1000)
        return 7;

    // Machine owns its worker lifetime; an SDL-style reset must stop and
    // rebase the clock, then restart it without introducing a second worker.
    Machine machine;
    machine.startRealtimePtm();
    if(!machine.realtimePtmEnabled())return 8;
    machine.pauseRealtimePtm();
    machine.resumeRealtimePtm();
    machine.reset();
    if(!machine.realtimePtmEnabled())return 9;
    machine.stopRealtimePtm();
    if(machine.realtimePtmEnabled())return 10;
    return 0;
}
