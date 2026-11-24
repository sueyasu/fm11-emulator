#include "Machine.h"
#include "Main6809Board.h"
#include "Main8088Board.h"
#include <cstdint>
#include <memory>

// A 10-bit 8N1 frame at 1000 baud lasts 10 ms. The same shared
// system-board USART must advance from either CPU's instruction cycles.
static int run(CpuType cpuType)
{
    Machine machine;
    Main6809Board* p6809=nullptr;
    Main8088Board* p8088=nullptr;
    if(cpuType==CpuType::MC6809) {
        auto board=std::make_unique<Main6809Board>(cpuType);
        p6809=board.get();
        Main6809Board::BootRomImage rom{};
        rom[0xffe]=0x10;rom[0xfff]=0x00; // RESET PC = 1000h
        board->loadBootRom(rom);
        machine.addBoard(std::move(board));
    } else {
        auto board=std::make_unique<Main8088Board>(cpuType);
        p8088=board.get();
        Main8088Bus::Rom rom{};
        rom.fill(0x90); // 8088 NOP
        board->bus().loadBootRom(rom);
        machine.addBoard(std::move(board));
        machine.set8088ClockHz(1000);
    }
    machine.reset();
    if(p6809) {
        p6809->setCpuClockHz(1000);
        for(unsigned i=0;i<64;++i)
            p6809->writeCpu(static_cast<WORD>(0x1000+i),0x12); // MC6809 NOP
        // Keep IRQ masked while stepping NOPs. Assert the 6809
        // aggregator later without vectoring into an uninitialized ISR.
    }
    auto uart=machine.systemBoard().usartDevice();
    // Default transport remains instantaneous; not clock-driven until opt-in.
    if(uart->baudRate()!=0)return 1;
    machine.setSerialClockBaud(1000);
    uart->writeCommand(0x4e); // async 8N1
    uart->writeCommand(0x04); // receiver only: no transmit IRQ
    uart->receive('R');
    for(unsigned i=0;i<3;++i)machine.executeOne();
    if(uart->rxReady() || uart->irqRequest())return 2;
    for(unsigned i=0;i<10;++i)machine.executeOne();
    if(!uart->rxReady() || !uart->irqRequest() ||
       (machine.systemBoard().irqLines()&0x10)==0)return 3;
    if(p6809) {
        p6809->writeCpu(0xfd02,0x04); // unmask after arrival
        p6809->syncSerialIrq();
        if(!p6809->irqPending())return 4;
        p6809->writeCpu(0xfd02,0x00); // re-mask before continuing NOPs
    }
    if(p8088) {
        p8088->bus().refreshSystemIrq();
        if(!p8088->bus().interruptPending())return 5;
    }
    if(uart->readData()!='R' || uart->rxReady())return 6;
    uart->writeCommand(0x01); // transmitter only
    uart->writeData('T');
    BYTE sent=0;
    if(uart->takeOutput(sent))return 7;
    for(unsigned i=0;i<3;++i)machine.executeOne();
    if(uart->takeOutput(sent))return 8;
    for(unsigned i=0;i<10;++i)machine.executeOne();
    if(!uart->takeOutput(sent) || sent!='T' ||
       uart->takeOutput(sent))return 9;
    // Explicit nanosecond stepping also advances serial, even without CPU.
    uart->writeCommand(0x04);
    uart->receive('S');
    machine.advanceTimeNanoseconds(9999999);
    if(uart->rxReady())return 10;
    machine.advanceTimeNanoseconds(1);
    if(!uart->rxReady() || uart->readData()!='S')return 11;
    machine.reset();
    if(uart->baudRate()!=1000 || uart->rxReady())return 12;
    machine.setSerialClockBaud(0);
    if(uart->baudRate()!=0)return 13;
    uart->writeCommand(0x4e);
    uart->writeCommand(0x05);
    uart->writeData('I');
    if(!uart->takeOutput(sent) || sent!='I')return 14;
    return 0;
}

int main()
{
    const int a=run(CpuType::MC6809);
    if(a)return a;
    const int b=run(CpuType::I8088);
    if(b)return 100+b;
    const int c=run(CpuType::NECV20);
    return c?200+c:0;
}
