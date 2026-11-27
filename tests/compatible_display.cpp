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
            kanji[0x681*32+i*2]=0x30;
            kanji[0x681*32+i*2+1]=0x70;
            kanji[0x97a*32+i*2]=0x31;
            kanji[0x97a*32+i*2+1]=0x71;
            kanji[0xedc*32+i*2]=0x32;
            kanji[0xedc*32+i*2+1]=0x72;
            kanji[0xd7c*32+i*2]=0x33;
            kanji[0xd7c*32+i*2+1]=0x73;
            kanji[0x90c*32+i*2]=0x34;
            kanji[0x90c*32+i*2+1]=0x74;
            kanji[0x401*32+i*2]=0x35;
            kanji[0x401*32+i*2+1]=0x75;
            kanji2[0x2*32+i*2]=0x37;
            kanji2[0x2*32+i*2+1]=0x77;
            kanji2[0x3*32+i*2]=0x38;
            kanji2[0x3*32+i*2+1]=0x78;
            kanji2[0x4*32+i*2]=0x39;
            kanji2[0x4*32+i*2+1]=0x79;
            kanji2[0x5*32+i*2]=0x3a;
            kanji2[0x5*32+i*2+1]=0x7a;
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
        auto call=[&](BYTE cmd,BYTE arg=0,BYTE low=0,BYTE aux=0) {
            wait();
            main->writeCpu(0xfd05,0x80);
            main->writeCpu(0xfc80,0);
            main->writeCpu(0xfc81,0);
            main->writeCpu(0xfc82,cmd);
            main->writeCpu(0xfc83,arg);
            main->writeCpu(0xfc84,low);
            main->writeCpu(0xfc85,aux);
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
        // Exact Shift-JIS sample bytes from NitrOS-9 kanjitest.
        main->writeCpu(0xfda0,0x0d);
        call(0x37,0xff);
        if(m.displaySharedByte(0)!=0)return 21;
        const BYTE samples[][4]={
            {0x8a,0xbf,0x30,0x70},
            {0x8e,0x9a,0x31,0x71},
            {0x93,0xfa,0x32,0x72},
            {0x96,0x7b,0x33,0x73},
            {0x8c,0xea,0x34,0x74},
            {0x88,0x9f,0x35,0x75},
            {0x98,0x9f,0x18,0x24},
            {0x98,0xa0,0x37,0x77},
            {0x98,0xa1,0x38,0x78},
            {0x98,0xa2,0x39,0x79},
            {0x98,0xa3,0x3a,0x7a}
        };
        unsigned x=0;
        for(const auto& s:samples) {
            const auto before=d.kanjiStatistics().selections;
            out(s[0]);
            if(d.kanjiStatistics().selections!=before ||
               d.textCursor().offset!=x*2)return 22;
            out(s[1]);
            if(d.kanjiStatistics().selections!=before+1 ||
               d.readPhysicalVram(x)!=s[2] ||
               d.readPhysicalVram(x+1)!=s[3] ||
               d.textCursor().offset!=(x+2)*2) {
                std::cerr<<"Shift-JIS pair failed at column "<<x<<"\n";
                return 23;
            }
            x+=2;
        }
        out('A');out(0xa6);
        if(d.textByte(x*2)!='A'||d.textByte((x+1)*2)!=0xa6)return 24;
        go(79,2);out(0x98);out(0x9f);
        if(d.readPhysicalVram(3*1280)!=0x18 ||
           d.textCursor().offset!=(3*80+2)*2)return 25;
        go(78,4);out(0x98);out(0x9f);
        if(d.readPhysicalVram(4*1280+78)!=0x18 ||
           d.textCursor().offset!=5*160)return 26;
        // Invalid trail reprocessed as a native control; no stale lead.
        out(0x98);out(0x0d);out('B');
        if(d.textByte(5*160)!='B')return 27;
        out(0x98);call(0x37,0);
        out(0x9f);
        if(d.textByte(5*160+2)!=0x9f)return 28;
        call(0x37,0xff);call(0x37,1);
        out(0x98);
        if(d.textByte(5*160+4)!=0x98)return 29;
        if(m.mainDisplayControl()!=0x0d)return 30;
        // Exercise metadata origin wrap and independent VRAM rings.
        out(0x1b);out('*');
        for(unsigned i=0;i<52;++i) {
            go(6,24);call(0x34,0x24,0x22);
            const auto addr=(d.graphicsDisplayAddress()+24*1280+6)&0x7fff;
            if(d.readPhysicalVram(addr)!=0x81 ||
               d.readPhysicalVram((addr+1)&0x7fff)!=0x42)return 31;
            go(0,24);out(0x0a);
            if(d.readPhysicalVram(addr)!=0x81)return 32;
            go(7,23);out('X');
            if(d.readPhysicalVram(addr)!=0 ||
               d.readPhysicalVram((addr+1)&0x7fff)!=0)return 33;
        }
        // Keep glyphs alive until they scroll off the top, across ring wrap.
        go(10,24);call(0x34,0x24,0x22);
        const auto old=(d.graphicsDisplayAddress()+24*1280+10)&0x7fff;
        for(unsigned i=0;i<24;++i) {
            go(0,24);out(0x0a);
            if(d.readPhysicalVram(old)!=0x81)return 34;
        }
        go(0,24);out(0x0a);
        if(d.readPhysicalVram(old)!=0 ||
           d.readPhysicalVram((old+1)&0x7fff)!=0)return 35;
        // Reverse rendering uses a distinct loop and must retain all rasters.
        out(0x1b);out('*');out(0x1b);out('G');out(0x0f);
        go(2,0);call(0x34,0x24,0x22);
        for(unsigned i=0;i<16;++i)
            if(d.readPhysicalVram(i*80+2)!=0x7e ||
               d.readPhysicalVram(i*80+3)!=0xbd)return 36;
        // Stock 6809 boot ROM hands off to IPL after HALT/release
        // without a command, leaving FC80=80. READY must reassert.
        out(0x1b);out('*');
        wait();
        main->writeCpu(0xfd05,0x80);
        main->writeCpu(0xfc80,0x80);
        main->writeCpu(0xfc82,0);
        for(unsigned i=3;i<0x33;++i)main->writeCpu(0xfc80+i,0);
        main->writeCpu(0xfd05,0);
        wait();
        if(m.displaySharedByte(0)!=0x80)return 37;
        // Command 03 is count-prefixed, not NUL-terminated.
        main->writeCpu(0xfd05,0x80);
        main->writeCpu(0xfc80,0x80);
        main->writeCpu(0xfc81,0);
        main->writeCpu(0xfc82,3);
        main->writeCpu(0xfc83,5);
        const BYTE msg[]={ 'R','O','M',0x0d,'!' };
        for(unsigned i=0;i<5;++i)main->writeCpu(0xfc84+i,msg[i]);
        main->writeCpu(0xfd05,0);
        wait();
        if(m.displaySharedByte(0)!=0 || d.textByte(0)!='!' ||
           d.textByte(2)!='O' || d.textByte(4)!='M' ||
           d.textCursor().offset!=2)return 38;
        call(0x55);
        if(m.displaySharedByte(0)!=0x46)return 13;
        std::cout<<"PASS: independent display firmware mailbox, text, scroll and keyboard\n";
        return 0;
    } catch(const std::exception& e) {
        std::cerr<<e.what()<<'\n';
        return 20;
    }
}
