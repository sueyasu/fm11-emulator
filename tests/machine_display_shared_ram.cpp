#include "Machine.h"
#include "Main6809Board.h"
#include <memory>

// Verify both processors see precisely one 128-byte shared command buffer.
int main() {
    auto shared=std::make_shared<MainBoard>();
    Main6809Board cpu(CpuType::MC6809);
    cpu.setMainBoard(shared);
    auto* main=&cpu;
    auto& sub=shared->displaySubsystem();
    for(unsigned i=0;i<128;++i) {
        const BYTE first=static_cast<BYTE>(i^0x5a);
        main->writeCpu(static_cast<WORD>(0xfc80+i),first);
        if(sub.readMemory(static_cast<WORD>(0x9f80+i))!=first)return 1;
        const BYTE second=static_cast<BYTE>(i^0xa5);
        sub.writeMemory(static_cast<WORD>(0x9f80+i),second);
        if(main->readCpu(static_cast<WORD>(0xfc80+i))!=second)return 2;
    }
    const auto previous=sub.sharedWriteRevision();
    sub.writeMemory(0x9f80,0x23);
    if(sub.sharedWriteRevision()!=previous+1)return 3;
    sub.writeMemory(0x9f80,0x23);
    if(sub.sharedWriteRevision()!=previous+1)return 4;
    // Adjacent private sub RAM must not alias the main-side shared buffer.
    const BYTE original=main->readCpu(0xfc80);
    sub.writeMemory(0x9f7f,static_cast<BYTE>(original^0xff));
    if(main->readCpu(0xfc80)!=original)return 5;
    return 0;
}
