#include "Main8088Board.h"
#include <array>
#include <iostream>

// Intel 8088 ModR/M TEST/XCHG: 84/85 read-only; 86/87 RMW.
// Exercise all EA forms, displacements, register operands, and both
// widths. The optional bus-event collector is enabled only in tests.
int main() {
    constexpr unsigned eaBase[8]={7,8,8,7,5,5,6,5};
    const BYTE opcodes[]={0x84,0x85,0x86,0x87};
    for(BYTE opcode:opcodes) {
        const bool word=(opcode&1)!=0;
        const bool exchange=opcode>=0x86;
        for(unsigned mod=0;mod<4;++mod) {
            for(unsigned rm=0;rm<8;++rm) {
                Main8088Board board(CpuType::I8088);
                const BYTE modrm=BYTE((mod<<6)|rm); // REG=AL/AX
                unsigned length=2;
                board.bus().writeMemory(0x10000,opcode);
                board.bus().writeMemory(0x10001,modrm);
                if(mod==0 && rm==6) {
                    board.bus().writeMemory(0x10002,0x00);
                    board.bus().writeMemory(0x10003,0x04);
                    length=4;
                } else if(mod==1) {
                    board.bus().writeMemory(0x10002,0x10);
                    length=3;
                } else if(mod==2) {
                    board.bus().writeMemory(0x10002,0x10);
                    board.bus().writeMemory(0x10003,0x00);
                    length=4;
                }
                std::array<WORD,8> regs{};
                regs[0]=0x1234;
                regs[3]=0x0200; // BX
                regs[4]=0x8000;
                regs[5]=0x0300; // BP
                regs[6]=0x0010; // SI
                regs[7]=0x0020; // DI
                const std::array<WORD,4> segments{0,0x1000,0,0};
                // All possible effective addresses are within [0, 0x400].
                // Give them safe readable/writable bytes.
                for(unsigned address=0x0100;address<=0x0501;++address)
                    board.bus().writeMemory(address,0x56);
                auto& cpu=static_cast<Main8088Board::Core&>(board.cpu());
                cpu.setConformanceState(regs,segments,0,2);
                cpu.setBusEventTrackingEnabled(true);
                cpu.executeOne();
                const bool memory=mod!=3;
                const unsigned ea=(mod==0 && rm==6)
                    ?6u:eaBase[rm]+(mod==0||mod==3?0u:4u);
                const unsigned base=exchange
                    ?(word?25u:17u):(word?13u:9u);
                const unsigned expected=memory
                    ?base+ea:(exchange?4u:3u);
                unsigned reads=0,writes=0;
                for(const auto& event:cpu.busEvents()) {
                    if(event.kind==Intel8088BusEvents::Kind::MemoryRead)
                        ++reads;
                    if(event.kind==Intel8088BusEvents::Kind::MemoryWrite)
                        ++writes;
                }
                const unsigned expectedBytes=memory?(word?2u:1u):0u;
                if(cpu.lastExecutionCycles()!=expected ||
                   cpu.ip()!=length ||
                   reads!=expectedBytes ||
                   writes!=(exchange?expectedBytes:0u)) {
                    std::cerr<<"opcode "<<std::hex<<unsigned(opcode)
                        <<" modrm "<<unsigned(modrm)<<std::dec
                        <<": cycles "<<cpu.lastExecutionCycles()
                        <<" expected "<<expected
                        <<", IP "<<cpu.ip()<<" expected "<<length
                        <<", read/write "<<reads<<"/"<<writes
                        <<" expected "<<expectedBytes<<"/"
                        <<(exchange?expectedBytes:0u)<<"\\n";
                    return 1;
                }
            }
        }
    }
    // Prefix costs only the current 2-clock estimate; it must not
    // alter the reference base clock class of TEST or XCHG.
    for(BYTE opcode:{BYTE(0x84),BYTE(0x85),BYTE(0x86),BYTE(0x87)}) {
        Main8088Board board(CpuType::I8088);
        board.bus().writeMemory(0x10000,0x26);
        board.bus().writeMemory(0x10001,opcode);
        board.bus().writeMemory(0x10002,0x07);
        std::array<WORD,8> regs{};
        regs[3]=0x0200;
        regs[4]=0x8000;
        const std::array<WORD,4> seg{0,0x1000,0,0};
        auto& cpu=static_cast<Main8088Board::Core&>(board.cpu());
        cpu.setConformanceState(regs,seg,0,2);
        cpu.executeOne();
        const unsigned expected=(opcode==0x84?9u:
            opcode==0x85?13u:opcode==0x86?17u:25u)+5u+2u;
        if(cpu.lastExecutionCycles()!=expected || cpu.ip()!=3)return 2;
    }
    return 0;
}
