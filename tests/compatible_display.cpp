#include "Machine.h"
#include "Main6809Board.h"
#include <iostream>
#include <memory>
#include <stdexcept>
int main(int argc,char** argv) {
    if(argc!=2)return 2;
    try {
        Machine m;
        m.loadDisplayRomFile(argv[1]);
        auto cpu=std::make_unique<Main6809Board>(CpuType::MC6809);
        auto* main=cpu.get();
        cpu->setCpuClockHz(2000000);
        m.addBoard(std::move(cpu));
        m.setDisplayCpuClockHz(2000000);
        m.reset();
        auto wait=[&] {
            for(int i=0;i<500000;++i) {
                if(main->readCpu(0xfd05)==0)return;
                m.executeOne();
            }
            throw std::runtime_error("display READY timeout");
        };
        wait();
        auto call=[&](BYTE cmd,BYTE arg=0) {
            wait();
            main->writeCpu(0xfd05,0x80);
            main->writeCpu(0xfc80,0);
            main->writeCpu(0xfc81,0);
            main->writeCpu(0xfc82,cmd);
            main->writeCpu(0xfc83,arg);
            main->writeCpu(0xfc84,0);
            main->writeCpu(0xfd05,0);
            wait();
        };
        auto out=[&](BYTE c){call(0x12,c);};
        auto go=[&](BYTE x,BYTE y){out(0x12);out(x);out(y);};
        const auto& d=m.displaySubsystem();
        if(d.crtcRegister(1)!=40||d.crtcRegister(6)!=25)return 3;
        out('A');out('B');
        if(d.textByte(0)!='A'||d.textByte(2)!='B'||d.textCursor().offset!=4)return 4;
        out(0x1b);out('G');out(0x1a);out('C');
        if(d.textByte(4)!='C'||d.textByte(5)!=0x1a)return 5;
        go(1,0);out(0x1b);out('T');
        if(d.textByte(0)!='A'||d.textByte(2)!=0||d.textCursor().offset!=2)return 6;
        go(0,24);out(0x0a);
        if(d.textStartAddress()!=160)return 7;
        out(0x1b);out('*');
        if(d.textStartAddress()!=0||d.textCursor().offset!=0)return 8;
        call(0x2d);
        if(m.displaySharedByte(3)!=0x80)return 9;
        m.enqueueDisplayKeyCode(0x0041);
        m.enqueueDisplayKeyCode(0x0116);
        for(int i=0;i<2000;++i)m.executeOne();
        call(0x2d);
        if(m.displaySharedByte(3)!=0||m.displaySharedByte(4)!=0x41)return 10;
        call(0x2d);
        if(m.displaySharedByte(3)!=1||m.displaySharedByte(4)!=0x16)return 11;
        call(0x2d);
        if(m.displaySharedByte(3)!=0x80)return 12;
        call(0x55);
        if(m.displaySharedByte(0)!=0x46)return 13;
        std::cout<<"PASS: independent display firmware mailbox, text, scroll and keyboard\n";
        return 0;
    } catch(const std::exception& e) {
        std::cerr<<e.what()<<'\n';
        return 20;
    }
}
