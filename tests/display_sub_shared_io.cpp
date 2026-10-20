#include "MainBoard.h"
#include "Main6809Board.h"
#include <memory>
int main() {
    auto board=std::make_shared<MainBoard>();
    Main6809Board main(CpuType::MC6809);
    main.setMainBoard(board);
    auto& sub=board->displaySubsystem();
    for(unsigned n=0;n<3;++n) {
        const BYTE x=static_cast<BYTE>(0x31+n);
        sub.writeMemory(static_cast<WORD>(0xaff0+n),x);
        if(main.readCpu(static_cast<WORD>(0xfd20+n))!=x)return 1;
        // Source GetIoByte(AFF0..AFF2) does not return the main register:
        // unhandled reads fall back to private SubRam storage.
        if(sub.readMemory(static_cast<WORD>(0xaff0+n))==x)return 2;
        const BYTE y=static_cast<BYTE>(0x51+n);
        main.writeCpu(static_cast<WORD>(0xfd20+n),y);
        if(sub.readMemory(static_cast<WORD>(0xaff0+n))==y)return 3;
        BYTE read=0;
        if(!board->readIo(static_cast<WORD>(0xfd20+n),read)||read!=y)return 4;
    }
    board->writeIo(0xfd04,0x80);
    if(sub.readMemory(0xaff3)!=0x80)return 5;
    sub.readMemory(0xafe7);
    if(sub.readMemory(0xaff3)!=0)return 6;
    board->resetSubCpuControl();
    for(unsigned n=0;n<3;++n) {
        BYTE read=0xff;
        if(!board->readIo(static_cast<WORD>(0xfd20+n),read)||read!=0)return 7;
    }
    return 0;
}
