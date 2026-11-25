#include "Machine.h"
#include "Main6809Board.h"
#include "Main8088Board.h"
#include <algorithm>
#include <cstdint>
#include <memory>
#include <vector>

int main() {
    Machine m;
    auto board=std::make_unique<Main6809Board>(CpuType::MC6809);
    auto* cpu=board.get();
    m.addBoard(std::move(board));
    auto board2=std::make_unique<Main8088Board>(CpuType::I8088);
    auto* x86=board2.get();
    m.addBoard(std::move(board2));
    auto sound=m.systemBoard().beeperDevice();
    m.reset();
    std::vector<std::int16_t> samples(480);
    // FD03 read remains CPU interrupt status; write is audio-only.
    cpu->writeCpu(0xfd03,0x81);
    if(!sound->active())return 1;
    sound->renderMono(samples.data(),samples.size(),48000);
    if(std::all_of(samples.begin(),samples.end(),[](auto v){return v==0;}))
        return 2;
    if(samples[0]!=5000 || samples[20]!=-5000 || samples[40]!=5000)
        return 3;
    cpu->writeCpu(0xfd03,0x00);
    if(sound->active())return 4;
    sound->renderMono(samples.data(),samples.size(),48000);
    if(std::any_of(samples.begin(),samples.end(),[](auto v){return v!=0;}))
        return 5;

    // Shared speaker: same result when controlled by the 8088/V20 bus.
    x86->bus().writeIo(0xfd03,0x81);
    if(!sound->active())return 6;
    x86->bus().writeIo(0xfd03,0x01);
    if(sound->active() || !sound->speakerEnabled())return 7;

    // Bit 6 starts a finite 205 ms beep, measured in output samples.
    x86->bus().writeIo(0xfd03,0x41);
    if(!sound->active())return 8;
    std::vector<std::int16_t> pulse(9840);
    sound->renderMono(pulse.data(),pulse.size(),48000);
    if(pulse.front()==0 || pulse.back()==0 || sound->active())return 9;
    sound->renderMono(samples.data(),samples.size(),48000);
    if(std::any_of(samples.begin(),samples.end(),[](auto v){return v!=0;}))
        return 10;
    // FD03 reads do not retrigger the tone.
    (void)cpu->readCpu(0xfd03);
    if(sound->active())return 11;
    cpu->writeCpu(0xfd03,0x81);
    if(!sound->active())return 12;
    m.reset();
    if(sound->active() || sound->speakerEnabled())return 13;
    return 0;
}
