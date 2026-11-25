#include "Machine.h"
#include "DisplayBitmap.h"
#include <array>
#include <cstdint>
#include <iostream>
#include <vector>

// An actual MC6809 sub-CPU program reads the Kanji ROM I/O ports and
// transfers each of the 16 left/right raster bytes to the graphics VRAM.
// No host-side readMemory() or writeMemory() is used for this transfer.
int main() {
    Machine m;
    DisplaySubsystem::KanjiRomImage kanji{};
    const unsigned code=3; // 32 bytes/glyph, interleaved left/right
    for(unsigned row=0;row<16;++row) {
        kanji[code*32+row*2]=BYTE(0x80u>>(row%8));
        kanji[code*32+row*2+1]=BYTE(0x01u<<(row%8));
    }
    m.systemBoard().displaySubsystem().loadKanjiRom(kanji);
    DisplaySubsystem::RomImage sub{};
    sub.fill(0x12); // NOP
    std::vector<BYTE> program;
    auto emit=[&](BYTE v){program.push_back(v);};
    // LDA #$00; STA $AFF4; LDA #$03; STA $AFF5
    const std::array<std::array<BYTE,5>,2> setup{{
        {{0x86,0x00,0xb7,0xaf,0xf4}},
        {{0x86,0x03,0xb7,0xaf,0xf5}}
    }};
    for(const auto& inst:setup)for(BYTE v:inst)emit(v);
    for(unsigned row=0;row<16;++row) {
        // LDA $AFF6; STA $0000+row*80; LDA $AFF7; STA $0001+row*80
        const unsigned offset=row*80;
        for(unsigned side=0;side<2;++side) {
            emit(0xb6);emit(0xaf);emit(BYTE(0xf6+side));
            emit(0xb7);emit(BYTE((offset+side)>>8));
            emit(BYTE(offset+side));
        }
    }
    // LDA #$77; STA $9F83. Signal that sub-CPU machine code completed.
    for(BYTE v:{BYTE(0x86),BYTE(0x77),BYTE(0xb7),BYTE(0x9f),
                BYTE(0x83),BYTE(0x20),BYTE(0xfe)})emit(v);
    if(program.size()>=sub.size()-2)return 1;
    std::copy(program.begin(),program.end(),sub.begin());
    // MC6809 reset vector FFFE = B000 (sub-ROM starts at B000).
    sub[0x4ffe]=0xb0;
    sub[0x4fff]=0x00;
    m.loadDisplayRom(sub);
    m.reset();
    // Drive the real sub-CPU, not the host-side display I/O directly.
    for(unsigned i=0;i<500 && m.displaySharedByte(3)!=0x77;++i)
        if(!m.systemBoard().displayCpu().executeOne())return 2;
    if(m.displaySharedByte(3)!=0x77)return 3;
    for(unsigned row=0;row<16;++row) {
        const unsigned offset=row*80;
        if(m.displayPhysicalVramByte(offset)!=(0x80u>>(row%8)) ||
           m.displayPhysicalVramByte(offset+1)!=(0x01u<<(row%8)))
            return 4;
    }
    if(m.displayPhysicalVramByte(2)!=0 ||
       m.displayPhysicalVramByte(80+2)!=0)return 5;
    // The same graphics path used by the SDL/software renderer must
    // reflect the guest-generated 16x16 raster, not a host Kanji overlay.
    const auto frame=m.captureDisplayFrame(false);
    if(frame.width!=640 || frame.height!=200)return 6;
    const auto black=DisplayBitmap::colorArgb(0);
    const auto foreground=DisplayBitmap::colorArgb(1);
    for(unsigned row=0;row<16;++row) {
        const unsigned left=row%8;
        const unsigned right=15-(row%8);
        const auto at=[&](unsigned x) {
            return frame.argb[row*640u+x];
        };
        if(at(left)!=foreground || at(right)!=foreground ||
           at(16)!=black)return 7;
    }
    std::cout<<"PASS: MC6809 sub-ROM -> AFF4/5 -> AFF6/7 -> VRAM -> graphics scanout\n";
    return 0;
}
