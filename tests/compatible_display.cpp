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
        DisplaySubsystem::KanjiRomImage kanji{}, kanji2{};
        for(unsigned i=0;i<16;++i) {
            kanji[0x82*32+i*2]=0x81;
            kanji[0x82*32+i*2+1]=0x42;
            kanji2[1*32+i*2]=0x18;
            kanji2[1*32+i*2+1]=0x24;
        }
        m.systemBoard().displaySubsystem().loadKanjiRom(kanji);
        m.systemBoard().displaySubsystem().loadKanji2Rom(kanji2);
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
        auto call=[&](BYTE cmd,BYTE arg=0,BYTE low=0) {
            wait();
            main->writeCpu(0xfd05,0x80);
            main->writeCpu(0xfc80,0);
            main->writeCpu(0xfc81,0);
            main->writeCpu(0xfc82,cmd);
            main->writeCpu(0xfc83,arg);
            main->writeCpu(0xfc84,low);
            main->writeCpu(0xfd05,0);
            wait();
        };
        auto out=[&](BYTE c){call(0x12,c);};
        auto go=[&](BYTE x,BYTE y){out(0x12);out(x);out(y);};
        const auto& d=m.displaySubsystem();
        if(d.crtcRegister(1)!=40||d.crtcRegister(6)!=25)return 3;
        const auto shape=d.textCursor();
        if(shape.first!=0 || shape.last!=15 || shape.mode!=2) {
            std::cerr<<"Incorrect cursor shape: first="<<shape.first
                     <<" last="<<shape.last<<" mode="<<shape.mode<<"\n";
            return 14;
        }
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
        // Host keys reach AFEC/AFED on the periodic display NMI (60000
        // sub-CPU cycles), not immediately on enqueue. Allow multiple ticks.
        for(int i=0;i<100000;++i)m.executeOne();
        call(0x2d);
        if(m.displaySharedByte(3)!=0||m.displaySharedByte(4)!=0x41) {
            std::cerr<<"First key: expected 0041, got "<<std::hex
                     <<unsigned(m.displaySharedByte(3))<<":"
                     <<unsigned(m.displaySharedByte(4))
                     <<" subPC="<<m.displayCpuPc()<<" FIFO head="
                     <<unsigned(m.displayMemory(0x9009))<<" tail="
                     <<unsigned(m.displayMemory(0x900a))<<"\n";
            return 10;
        }
        call(0x2d);
        if(m.displaySharedByte(3)!=1||m.displaySharedByte(4)!=0x16) {
            std::cerr<<"Second key: expected 0116, got "<<std::hex
                     <<unsigned(m.displaySharedByte(3))<<":"
                     <<unsigned(m.displaySharedByte(4))<<"\n";
            return 11;
        }
        call(0x2d);
        if(m.displaySharedByte(3)!=0x80)return 12;
        // Layer enable is owned by the main CPU, not the sub-ROM.
        out(0x1b);out('G');out(7);
        main->writeCpu(0xfda0,3);
        go(2,1);call(0x34,0x24,0x22);
        if(m.mainDisplayControl()!=3 || d.textCursor().offset!=164 ||
           d.readPhysicalVram(1282)!=0x81 || d.readPhysicalVram(1283)!=0x42)
            return 15;
        main->writeCpu(0xfda0,15);
        go(4,1);call(0x34,0x50,0x21);
        if(d.readPhysicalVram(1284)!=0x18 || d.readPhysicalVram(1285)!=0x24)
            return 16;
        go(3,1);out('X');
        if(d.readPhysicalVram(1282)!=0 || d.readPhysicalVram(1283)!=0)
            return 17;
        go(0,24);out(0x0a);
        if(d.graphicsDisplayAddress()!=1280 || d.textStartAddress()!=160 ||
           d.readPhysicalVram(1284)!=0x18)return 18;
        out(0x1b);out('*');
        if(d.graphicsDisplayAddress()!=0 || d.readPhysicalVram(1284)!=0)
            return 19;
        call(0x55);
        if(m.displaySharedByte(0)!=0x46)return 13;
        std::cout<<"PASS: independent display firmware mailbox, text, scroll and keyboard\n";
        return 0;
    } catch(const std::exception& e) {
        std::cerr<<e.what()<<'\n';
        return 20;
    }
}
