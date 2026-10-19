#include "MainBoard.h"
#include "DisplayBitmap.h"
#include "Machine.h"
#include "Main6809Board.h"
#include <memory>
#include <vector>

int main()
{
    MainBoard board;
    DisplaySubsystem::RomImage rom{};
    // C000: LDA #55; STA $8000; NOP
    rom[0x1000]=0x86; rom[0x1001]=0x55;
    rom[0x1002]=0xb7; rom[0x1003]=0x80; rom[0x1004]=0x00;
    rom[0x1005]=0x12;
    rom[0x4ffe]=0xc0; rom[0x4fff]=0x00;
    board.displaySubsystem().loadRom(rom);
    board.reset();
    auto& cpu=board.displayCpu();
    if(cpu.pc()!=0xc000)return 1;
    if(!cpu.executeOne() || cpu.pc()!=0xc002)return 2;
    if(!cpu.executeOne() || cpu.pc()!=0xc005)return 3;
    if(board.displaySubsystem().readMemory(0x8000)!=0x55)return 4;
    // HALT request via shared FD05 prevents further instructions.
    board.writeIo(0xfd05,0x80);
    if(cpu.executeOne() || cpu.pc()!=0xc005)return 5;
    // HALT release resumes from the suspended instruction boundary.
    board.writeIo(0xfd05,0x00);
    if(board.displaySubsystem().haltRequested() ||
       (board.subCpuStatus()&0x80)!=0)return 29;
    if(!cpu.executeOne() || cpu.pc()!=0xc006)return 30;
    board.reset();
    if(cpu.pc()!=0xc000 || board.displaySubsystem().haltRequested())return 6;
    if(board.displaySubsystem().readMemory(0x8000)!=0x55)return 7;
    // Elapsed-time scheduling is opt-in, and preserves CPU instruction
    // boundaries rather than executing one sub-instruction per main step.
    Machine timed;
    DisplaySubsystem::RomImage timedRom{};
    timedRom[0x1000]=0x86;timedRom[0x1001]=0x55;  // LDA #55
    timedRom[0x1002]=0xb7;timedRom[0x1003]=0x80;timedRom[0x1004]=0x00; // STA $8000
    timedRom[0x1005]=0x12; // NOP
    timedRom[0x4ffe]=0xc0;timedRom[0x4fff]=0x00;
    // The main-board display CPU is accessible through a card attachment.
    auto cpuBoard=std::make_unique<Main6809Board>(CpuType::MC6809);
    auto* mainCard=cpuBoard.get();
    timed.addBoard(std::move(cpuBoard));
    // Configure ROM through the same shared main board attached to 6809.
    // The display CPU is exposed directly by Machine for scheduler tests.
    timed.loadDisplayRom(timedRom);
    timed.reset();
    timed.advanceTimeNanoseconds(10000);
    if(timed.displayMemory(0x8000)!=0)return 8;
    timed.setDisplayCpuClockHz(1000000);
    timed.advanceTimeNanoseconds(2000);
    if(timed.displayCpuPc()!=0xc002)return 9;
    timed.advanceTimeNanoseconds(5000);
    if(timed.displayMemory(0x8000)!=0x55)return 10;
    mainCard->writeCpu(0xfd05,0x80);
    const auto haltedPc=timed.displayCpuPc();
    timed.advanceTimeNanoseconds(100000);
    if(timed.displayCpuPc()!=haltedPc)return 11;
    timed.reset();
    if(timed.displayCpuPc()!=0xc000)return 12;
    // Main-CPU instruction timing also advances the sub CPU.
    mainCard->setCpuClockHz(1000000);
    mainCard->writePhysical(0,0x12); // MC6809 NOP, two cycles
    timed.executeOne();
    if(timed.displayCpuPc()!=0xc002)return 13;
    // Main $FC80-$FCFF and sub $9F80-$9FFF address the same RAM.
    mainCard->writeCpu(0xfc80,0x61);
    mainCard->writeCpu(0xfcff,0x72);
    if(timed.displayMemory(0x9f80)!=0x61 ||
       timed.displayMemory(0x9fff)!=0x72)return 14;
    // Execute sub-CPU writes through the ROM instruction path, too.
    DisplaySubsystem::RomImage mailboxRom{};
    // LDA #A5; STA $9F81
    mailboxRom[0x1000]=0x86;mailboxRom[0x1001]=0xa5;
    mailboxRom[0x1002]=0xb7;mailboxRom[0x1003]=0x9f;mailboxRom[0x1004]=0x81;
    mailboxRom[0x4ffe]=0xc0;mailboxRom[0x4fff]=0x00;
    timed.loadDisplayRom(mailboxRom);
    timed.reset();
    timed.advanceTimeNanoseconds(7000);
    if(mainCard->readCpu(0xfc81)!=0xa5)return 15;
    // System reset does not erase RAM, and adjacent main-board RAM
    // remains independent from the shared aperture.
    mainCard->writeCpu(0xfc7f,0x33);
    mainCard->writeCpu(0xfd05,0x80);
    mainCard->reset();
    if(mainCard->readCpu(0xfc80)!=0x61)return 16;
    if(mainCard->readCpu(0xfc7f)!=0)return 17;
    // Sub-system I/O: AFE0-AFE3 scratch and AFF0 busy latch.
    // Unimplemented neighbors remain unmapped; AFEF is now CRTC data.
    auto& io=board.displaySubsystem();
    io.writeMemory(0xafe0,0x12);
    io.writeMemory(0xafe3,0x34);
    if(io.readMemory(0xafe0)!=0x12 ||
       io.readMemory(0xafe3)!=0x34)return 18;
    if(io.readMemory(0xafe4)!=0xff ||
       io.readMemory(0xafed)!=0xff ||
       io.readMemory(0xaff1)!=0xff)return 19;
    io.writeMemory(0xaff0,0x00);
    if(io.busyStatus()!=0 || io.readMemory(0xaff0)!=0)return 20;
    // BUSY control is accessible from real sub-CPU instructions.
    DisplaySubsystem::RomImage busyRom{};
    busyRom[0x1000]=0x86;busyRom[0x1001]=0x00; // LDA #$00
    busyRom[0x1002]=0xb7;busyRom[0x1003]=0xaf;busyRom[0x1004]=0xf0; // STA $AFF0
    busyRom[0x4ffe]=0xc0;busyRom[0x4fff]=0x00;
    io.loadRom(busyRom);
    board.reset();
    if(io.busyStatus()!=0x80 || io.readMemory(0xafe0)!=0)return 21;
    if(!board.displayCpu().executeOne() ||
       !board.displayCpu().executeOne() ||
       io.busyStatus()!=0x00)return 22;
    io.writeMemory(0xaff0,0);
    if(io.busyStatus()!=0)return 23;
    // Display-side ATTENTION reaches the common FD04 event latch.
    // FD04 read acknowledges the request; a second read is clear.
    board.displaySubsystem().notifyMainAttention();
    BYTE event=0;
    if(!board.readIo(0xfd04,event) || (event&0x01)==0)return 24;
    if(!board.readIo(0xfd04,event) || (event&0x01)!=0)return 25;
    // BUSY can be sampled by the main board without assuming yet
    // whether or how it shares FD05's HALT/status bit.
    board.displaySubsystem().writeMemory(0xaff0,0x37);
    if(board.displayBusyStatus()!=0x37)return 26;
    board.displaySubsystem().notifyMainAttention();
    board.reset();
    if(board.displayBusyStatus()!=0x80)return 27;
    if(!board.readIo(0xfd04,event) || (event&0x01)!=0)return 28;
    // A main CPU clock change must retain sub-nanosecond elapsed time:
    // two cycles at 3 MHz (666 2/3 ns) plus two at 4 MHz (500 ns)
    // exceed one display CPU cycle at 1 MHz.
    timed.loadDisplayRom(timedRom);
    timed.reset();
    timed.setDisplayCpuClockHz(1000000);
    mainCard->writePhysical(0,0x12);
    mainCard->writePhysical(1,0x12);
    mainCard->setCpuClockHz(3000000);
    timed.executeOne();
    if(timed.displayCpuPc()!=0xc000)return 31;
    mainCard->setCpuClockHz(4000000);
    timed.executeOne();
    if(timed.displayCpuPc()!=0xc002)return 32;
    // Repeated configuration with the same frequency is idempotent:
    // the first half-microsecond has not yet earned an instruction.
    timed.reset();
    timed.setDisplayCpuClockHz(1000000);
    timed.advanceTimeNanoseconds(500);
    if(timed.displayCpuPc()!=0xc000)return 33;
    timed.setDisplayCpuClockHz(1000000);
    timed.advanceTimeNanoseconds(500);
    if(timed.displayCpuPc()!=0xc002)return 34;
    // Preserve negative instruction-cycle credit across a redundant
    // configuration update as well (LDA costs two 6809 cycles).
    timed.reset();
    timed.setDisplayCpuClockHz(1000000);
    timed.advanceTimeNanoseconds(1000);
    if(timed.displayCpuPc()!=0xc002)return 35;
    timed.setDisplayCpuClockHz(1000000);
    timed.advanceTimeNanoseconds(1000);
    if(timed.displayCpuPc()!=0xc002)return 36;
    // AFE6 is display-side video control, not generic unmapped I/O.
    DisplaySubsystem::RomImage videoRom{};
    // LDA #$5A; STA $AFE6
    videoRom[0x1000]=0x86;videoRom[0x1001]=0x5a;
    videoRom[0x1002]=0xb7;videoRom[0x1003]=0xaf;videoRom[0x1004]=0xe6;
    videoRom[0x4ffe]=0xc0;videoRom[0x4fff]=0x00;
    board.displaySubsystem().loadRom(videoRom);
    board.reset();
    if(board.displaySubsystem().videoFlags()!=0)return 37;
    if(!board.displayCpu().executeOne() ||
       !board.displayCpu().executeOne())return 38;
    // GetIoByte($AFE6) toggles bits 7/2 and forces bit 5 on read.
    if(board.displaySubsystem().readMemory(0xafe6)!=0xfe ||
       board.displaySubsystem().readMemory(0xafe6)!=0x7a ||
       board.displaySubsystem().videoFlags()!=0x5a)return 39;
    // AFE5 is unmapped, but reading AFE7 signals ATTENTION to the
    // main board (original SubCpu.cpp). It is not a mirror of AFE6.
    if(board.displaySubsystem().readMemory(0xafe5)!=0xff)return 40;
    if(board.eventStatus()!=0)return 40;
    if(board.displaySubsystem().readMemory(0xafe7)!=0x00 ||
       (board.eventStatus()&0x01)==0)return 40;
    BYTE attentionStatus=0;
    if(!board.readIo(0xfd04,attentionStatus) ||
       (attentionStatus&0x01)==0 ||
       board.eventStatus()!=0)return 40;
    board.reset();
    if(board.displaySubsystem().videoFlags()!=0)return 41;
    // Video clients can detect changes without interpreting AFE6 bits.
    DisplaySubsystem changes;
    const auto initial=changes.videoRevision();
    changes.writeMemory(0x0000,0x80);
    if(changes.videoRevision()!=initial+1)return 42;
    changes.writeMemory(0x0000,0x80); // unchanged byte
    if(changes.videoRevision()!=initial+1)return 43;
    changes.writeMemory(0x7fff,0x01);
    if(changes.videoRevision()!=initial+2)return 44;
    changes.writeMemory(0x8000,0x11); // regular RAM is not video
    changes.writeMemory(0xaff0,0); // BUSY is not video
    if(changes.videoRevision()!=initial+2)return 45;
    changes.writeMemory(0xafe6,0x40);
    if(changes.videoRevision()!=initial+3)return 46;
    changes.writeMemory(0xafe6,0x40);
    if(changes.videoRevision()!=initial+3)return 47;
    changes.reset(); // control returns to zero, VRAM remains intact
    if(changes.videoRevision()!=initial+4 ||
       changes.readMemory(0x0000)!=0x80)return 48;
    changes.reset();
    if(changes.videoRevision()!=initial+4)return 49;
    // A frontend snapshot is an owning copy, not a live VRAM reference.
    Machine videoMachine;
    const auto before=videoMachine.captureDisplayVideo();
    if(before.vram[0]!=0 || before.flags!=0)return 50;
    const auto captured=changes.captureVideo();
    if(captured.vram[0]!=0x80 || captured.vram[0x7fff]!=0x01 ||
       captured.flags!=0 || captured.revision!=changes.videoRevision())
        return 51;
    changes.writeMemory(0x0000,0x42);
    changes.writeMemory(0xafe6,0x20);
    if(captured.vram[0]!=0x80 || captured.flags!=0 ||
       captured.revision==changes.videoRevision())return 52;
    const auto updated=changes.captureVideo();
    if(updated.vram[0]!=0x42 || updated.flags!=0x20 ||
       updated.revision!=changes.videoRevision())return 53;
    // Diagnostic 1-bpp output: first/last pixel, MSB-first bit order,
    // scanline stride and nonzero source plane offset.
    DisplaySubsystem pixels;
    pixels.writeMemory(0x0000,0x81); // x=0 and x=7
    pixels.writeMemory(0x0050,0x40); // second line, x=1
    pixels.writeMemory(0x3e7f,0x01); // final pixel (row 199, x=639)
    const auto mono=DisplayBitmap::decodeMonochrome(pixels.captureVideo());
    if(mono[0]!=1 || mono[1]!=0 || mono[7]!=1 ||
       mono[8]!=0 || mono[DisplayBitmap::kWidth+1]!=1 ||
       mono[DisplayBitmap::kWidth*DisplayBitmap::kHeight-1]!=1)
        return 54;
    pixels.writeMemory(0x0000,0);
    if(mono[0]!=1)return 55; // decoded pixels own their storage
    pixels.writeMemory(0x4000,0x80);
    const auto secondPlane=DisplayBitmap::decodeMonochrome(
        pixels.captureVideo(),0x4000);
    if(secondPlane[0]!=1 || secondPlane[7]!=0)return 56;
    const auto invalid=DisplayBitmap::decodeMonochrome(
        pixels.captureVideo(),0x4200);
    if(invalid[0]!=0 || invalid[7]!=0)return 57;
    // Diagnostic two-plane bit indices; no physical palette is assumed.
    DisplaySubsystem planes;
    planes.writeMemory(0x0000,0x50); // bit 0 at x=1 and x=3
    planes.writeMemory(0x4000,0x30); // bit 1 at x=2 and x=3
    planes.writeMemory(0x3e7f,0x01);
    planes.writeMemory(0x7e7f,0x01);
    const auto indexes=DisplayBitmap::decodeTwoPlanes(planes.captureVideo());
    if(indexes[0]!=0 || indexes[1]!=1 ||
       indexes[2]!=2 || indexes[3]!=3 || indexes[4]!=0)return 58;
    if(indexes[DisplayBitmap::kWidth*DisplayBitmap::kHeight-1]!=3)
        return 59;
    // A snapshot is immutable even if either source plane changes.
    planes.writeMemory(0,0);
    planes.writeMemory(0x4000,0);
    if(indexes[1]!=1 || indexes[2]!=2 || indexes[3]!=3)return 60;
    // Physical FM-11 VRAM is 192 KiB behind a 32 KiB CPU aperture.
    // Window selection here is an internal diagnostic mechanism; no
    // unverified hardware bank-select port is exposed yet.
    DisplaySubsystem banked;
    if(DisplaySubsystem::kPhysicalVramSize!=0x30000 ||
       DisplaySubsystem::kVramWindows!=6)return 61;
    banked.writeMemory(0,0x11);
    banked.writeMemory(0x7fff,0x12);
    const auto firstBank=banked.captureVideo();
    for(std::size_t window=1;window<DisplaySubsystem::kVramWindows;++window) {
        const auto previous=banked.videoRevision();
        banked.selectVramWindow(window);
        if(banked.videoRevision()!=previous+1 ||
           banked.readMemory(0)!=0)return 62;
        banked.writeMemory(0,static_cast<BYTE>(0x20+window));
        banked.writeMemory(0x7fff,static_cast<BYTE>(0x30+window));
        if(banked.readPhysicalVram(window*0x8000)!=0x20+window)
            return 63;
    }
    const auto finalBank=banked.captureVideo();
    if(finalBank.vram[0]!=0x25 || finalBank.vram[0x7fff]!=0x35)
        return 64;
    banked.selectVramWindow(0);
    if(banked.readMemory(0)!=0x11 || banked.readMemory(0x7fff)!=0x12)
        return 65;
    if(firstBank.vram[0]!=0x11 || firstBank.vram[0x7fff]!=0x12)
        return 66;
    const auto unchanged=banked.videoRevision();
    banked.selectVramWindow(0);
    if(banked.videoRevision()!=unchanged)return 67;
    bool invalidBank=false;
    try {banked.selectVramWindow(6);}
    catch(const std::out_of_range&) {invalidBank=true;}
    if(!invalidBank || banked.vramWindow()!=0)return 68;
    // Four candidate 48KiB color pages, three 16KiB planes each.
    // Set the first pixel of each page to a unique RGB index and
    // verify no data leaks between the physical VRAM pages.
    DisplaySubsystem colorRam;
    if(DisplayBitmap::kColorPages!=4)return 69;
    for(std::size_t page=0;page<DisplayBitmap::kColorPages;++page) {
        const unsigned index=static_cast<unsigned>(page)+1;
        for(std::size_t plane=0;plane<3;++plane) {
            const auto physical=page*0xc000+plane*0x4000;
            colorRam.selectVramWindow(physical/0x8000);
            colorRam.writeMemory(static_cast<WORD>(physical%0x8000),
                (index&(1u<<plane))?0x80:0);
        }
    }
    for(std::size_t page=0;page<DisplayBitmap::kColorPages;++page) {
        const auto rgb=DisplayBitmap::decodeColorPage(colorRam,page);
        if(rgb[0]!=page+1 || rgb[1]!=0 ||
           rgb[DisplayBitmap::kWidth*DisplayBitmap::kHeight-1]!=0)
            return 70;
    }
    bool invalidPage=false;
    try {DisplayBitmap::decodeColorPage(colorRam,4);}
    catch(const std::out_of_range&) {invalidPage=true;}
    if(!invalidPage)return 71;
    // 640x400 diagnostics: two independent 96KiB pages with three planes.
    DisplaySubsystem highRam;
    if(DisplayBitmap::kHighPages!=2 ||
       DisplayBitmap::kHighPageBytes!=0x18000)return 72;
    for(std::size_t page=0;page<2;++page) {
        const auto base=page*DisplayBitmap::kHighPageBytes;
        for(std::size_t plane=0;plane<3;++plane) {
            const auto first=base+plane*DisplayBitmap::kHighPlaneBytes;
            const auto last=first+399*DisplayBitmap::kBytesPerLine+79;
            highRam.selectVramWindow(first/DisplaySubsystem::kVramSize);
            highRam.writeMemory(static_cast<WORD>(
                first%DisplaySubsystem::kVramSize),0x80);
            highRam.selectVramWindow(last/DisplaySubsystem::kVramSize);
            highRam.writeMemory(static_cast<WORD>(
                last%DisplaySubsystem::kVramSize),0x01);
        }
    }
    for(std::size_t page=0;page<2;++page) {
        const auto pixel=DisplayBitmap::decodeHighColorPage(highRam,page);
        if(pixel[0]!=7 || pixel[1]!=0 ||
           pixel[DisplayBitmap::kWidth*DisplayBitmap::kHighHeight-1]!=7)
            return 73;
    }
    bool invalidHigh=false;
    try {DisplayBitmap::decodeHighColorPage(highRam,2);}
    catch(const std::out_of_range&) {invalidHigh=true;}
    if(!invalidHigh)return 74;
    // Trace sub-CPU I/O writes executed from ROM, including an unknown
    // location. The observer must not accidentally implement the port.
    MainBoard traced;
    DisplaySubsystem::RomImage traceRom{};
    // LDA #$12; STA $AFE6; STA $AFE4; STA $AFF0
    traceRom[0x1000]=0x86;traceRom[0x1001]=0x12;
    traceRom[0x1002]=0xb7;traceRom[0x1003]=0xaf;traceRom[0x1004]=0xe6;
    traceRom[0x1005]=0xb7;traceRom[0x1006]=0xaf;traceRom[0x1007]=0xe4;
    traceRom[0x1008]=0xb7;traceRom[0x1009]=0xaf;traceRom[0x100a]=0xf0;
    traceRom[0x4ffe]=0xc0;traceRom[0x4fff]=0x00;
    traced.displaySubsystem().loadRom(traceRom);
    std::vector<DisplaySubsystem::IoWriteTrace> log;
    traced.displaySubsystem().setIoWriteObserver(
        [&](DisplaySubsystem::IoWriteTrace entry){log.push_back(entry);});
    traced.reset();
    for(int i=0;i<4;++i)
        if(!traced.displayCpu().executeOne())return 75;
    if(log.size()!=3 || log[0].address!=0xafe6 ||
       log[1].address!=0xafe4 || log[2].address!=0xaff0 ||
       log[0].value!=0x12 || log[1].value!=0x12 ||
       log[2].value!=0x12)return 76;
    if(traced.displaySubsystem().readMemory(0xafe4)!=0xff ||
       traced.displaySubsystem().videoFlags()!=0x12 ||
       traced.displaySubsystem().busyStatus()!=0x12)return 77;
    traced.displaySubsystem().clearIoWriteObserver();
    traced.displaySubsystem().writeMemory(0xafe4,0x99);
    if(log.size()!=3)return 78;
    // The PC identifies the instruction that performed each I/O write.
    if(log[0].pc!=0xc002 || log[1].pc!=0xc005 ||
       log[2].pc!=0xc008 ||
       !log[0].fromCpu || !log[1].fromCpu || !log[2].fromCpu)
        return 79;
    traced.displaySubsystem().setIoWriteObserver(
        [&](DisplaySubsystem::IoWriteTrace entry){log.push_back(entry);});
    traced.displaySubsystem().writeMemory(0xafe4,0x77);
    if(log.size()!=4 || log.back().fromCpu)return 80;
    // Reference SubCpu.cpp graphics access: three 64KiB planes and
    // AFE1 plane write / preserve controls, AFE3 page, AFE8/9 offset.
    DisplaySubsystem gvu;
    gvu.writeMemory(0xafe1,0x07); // broadcast same byte to all planes
    gvu.writeMemory(0x0000,0xa5);
    if(gvu.readPhysicalVram(0)!=0xa5 ||
       gvu.readPhysicalVram(0x10000)!=0xa5 ||
       gvu.readPhysicalVram(0x20000)!=0xa5)return 81;
    gvu.writeMemory(0xafe1,0x51); // plane 1 read; plane 0 write; preserve plane 1
    gvu.writeMemory(0x0000,0x3c);
    if(gvu.readPhysicalVram(0)!=0x3c ||
       gvu.readPhysicalVram(0x10000)!=0xa5 ||
       gvu.readPhysicalVram(0x20000)!=0)return 82;
    if(gvu.readMemory(0x0000)!=0xa5 || gvu.graphicsReadPlane()!=1)
        return 83;
    gvu.writeMemory(0xafe3,0x18); // access page 3
    gvu.writeMemory(0xafe8,0x00);
    gvu.writeMemory(0xafe9,0x03); // address offset aligned to even
    gvu.writeMemory(0xafe1,0x04); // write only third plane
    gvu.writeMemory(0x0000,0x66);
    if(gvu.graphicsAccessPage()!=3 || gvu.graphicsOffset()!=2 ||
       gvu.readPhysicalVram(0x20000+0xc002)!=0x66)return 84;
    gvu.writeMemory(0xafe2,0x10);
    if(gvu.graphicsDisplayPage()!=2)return 85;
    gvu.reset();
    if(gvu.graphicsAccessPage()!=0 || gvu.graphicsDisplayPage()!=0 ||
       gvu.graphicsOffset()!=0)return 86;
    // Actual physical layout: R/G/B bits are in distinct 64KiB planes.
    // Select access page 1, then choose display page 1 independently.
    DisplaySubsystem realLayout;
    realLayout.writeMemory(0xafe3,0x08);
    for(unsigned plane=0;plane<3;++plane) {
        realLayout.writeMemory(0xafe1,static_cast<BYTE>(0x38|(1u<<plane)));
        realLayout.writeMemory(0,0x80);
    }
    const auto beforePageSwitch=realLayout.videoRevision();
    realLayout.writeMemory(0xafe2,0x08);
    if(realLayout.videoRevision()!=beforePageSwitch+1)return 92;
    realLayout.writeMemory(0xafe2,0x08);
    if(realLayout.videoRevision()!=beforePageSwitch+1)return 93;
    const auto pageOne=DisplayBitmap::decodeSelectedGraphics200(realLayout);
    const auto pageZero=DisplayBitmap::decodeGraphics200(realLayout,0);
    if(pageOne[0]!=7 || pageOne[1]!=0 || pageZero[0]!=0)
        return 87;
    if(realLayout.readPhysicalVram(0x4000)!=0x80 ||
       realLayout.readPhysicalVram(0x14000)!=0x80 ||
       realLayout.readPhysicalVram(0x24000)!=0x80)
        return 88;
    // 400-line page 1 spans 32KiB in each of the same three planes.
    realLayout.writeMemory(0xafe3,0x10);
    realLayout.writeMemory(0xafe1,0x3f);
    realLayout.writeMemory(0,0x40);
    realLayout.writeMemory(0xafe2,0x10);
    const auto highPage=DisplayBitmap::decodeSelectedGraphics400(realLayout);
    if(highPage[1]!=7 || highPage[0]!=0 ||
       DisplayBitmap::decodeGraphics400(realLayout,0)[1]!=0)
        return 89;
    bool rejected=false;
    try {DisplayBitmap::decodeGraphics200(realLayout,4);}
    catch(const std::out_of_range&) {rejected=true;}
    if(!rejected)return 90;
    rejected=false;
    try {DisplayBitmap::decodeGraphics400(realLayout,2);}
    catch(const std::out_of_range&) {rejected=true;}
    if(!rejected)return 91;
    // Display origin and indexed MC6845 register file.
    DisplaySubsystem crtc;
    crtc.writeMemory(0xafee,12);
    crtc.writeMemory(0xafef,0x34);
    crtc.writeMemory(0xafee,13);
    crtc.writeMemory(0xafef,0x56);
    if(crtc.crtcRegister(12)!=0x34 || crtc.crtcRegister(13)!=0x56 ||
       crtc.readMemory(0xafef)!=0x56)return 94;
    crtc.writeMemory(0xafee,31);
    crtc.writeMemory(0xafef,0xff);
    if(crtc.readMemory(0xafef)!=0xff || crtc.crtcRegister(12)!=0x34)
        return 95;
    crtc.writeMemory(0xafea,0x80);
    crtc.writeMemory(0xafeb,0x02);
    if(crtc.graphicsDisplayAddress()!=2)return 96;
    crtc.writeMemory(0xafe1,0x07);
    crtc.writeMemory(2,0x80);
    auto shifted=DisplayBitmap::decodeGraphics200(crtc,0);
    if(shifted[0]!=7 || shifted[1]!=0)return 97;
    crtc.writeMemory(0xafeb,0);
    auto unshifted=DisplayBitmap::decodeGraphics200(crtc,0);
    if(unshifted[0]!=0 || unshifted[16]!=7)return 98;
    const auto rev=crtc.videoRevision();
    crtc.writeMemory(0xafeb,0);
    if(crtc.videoRevision()!=rev)return 99;
    crtc.reset();
    if(crtc.graphicsDisplayAddress()!=0 || crtc.crtcRegister(12)!=0)
        return 100;
    // External character ROM and renderer primitives do not require
    // guessing the hardware text-mode register mapping.
    DisplaySubsystem fonts;
    DisplaySubsystem::AnkRomImage ank{};
    ank[0x41*16]=0x81;
    ank[0x41*16+15]=0x42;
    fonts.loadAnkRom(ank);
    DisplaySubsystem::KanjiRomImage kanji{};
    kanji[0x1ffff]=0x5a;
    fonts.loadKanjiRom(kanji);
    if(fonts.ankGlyphRow(0x41,0)!=0x81 ||
       fonts.ankGlyphRow(0x41,15)!=0x42 ||
       fonts.kanjiRomByte(0x1ffff)!=0x5a)return 101;
    DisplayBitmap::Pixels textPixels{};
    DisplayBitmap::drawAnk(textPixels,fonts,0x41,0,0,7,1);
    if(textPixels[0]!=7 || textPixels[1]!=1 ||
       textPixels[7]!=7 || textPixels[15*640+1]!=7 ||
       textPixels[15*640+6]!=7 ||
       textPixels[16*640]!=0)return 102;
    const auto argb=DisplayBitmap::toArgb(textPixels);
    if(argb[0]!=0xffffffffu || argb[1]!=0xffff0000u ||
       argb[16*640]!=0xff000000u)return 103;
    fonts.reset(); // Reset preserves physical font ROMs.
    if(fonts.ankGlyphRow(0x41,0)!=0x81 ||
       fonts.kanjiRomByte(0x1ffff)!=0x5a)return 104;
    // End-to-end frontend capture: selected physical page, ARGB pixel
    // ordering, explicit resolution, frame revision and owned buffers.
    Machine framed;
    auto displayBoard=std::make_unique<Main6809Board>(CpuType::MC6809);
    framed.addBoard(std::move(displayBoard));
    auto& framebuffer=framed.board(0);
    (void)framebuffer;
    // The CPU-board-independent renderer is also usable standalone.
    DisplaySubsystem frameSource;
    frameSource.writeMemory(0xafe1,0x07);
    frameSource.writeMemory(0x0000,0x80);
    const auto capturedFrame=DisplayBitmap::captureFrame(frameSource,false);
    if(capturedFrame.width!=640 || capturedFrame.height!=200 ||
       capturedFrame.argb.size()!=640*200 ||
       capturedFrame.argb[0]!=0xffffffffu ||
       capturedFrame.argb[1]!=0xff000000u ||
       capturedFrame.revision!=frameSource.videoRevision())return 105;
    frameSource.writeMemory(0x0000,0);
    if(capturedFrame.argb[0]!=0xffffffffu)return 106;
    frameSource.writeMemory(0xafe3,0x10);
    frameSource.writeMemory(0x0000,0x40);
    frameSource.writeMemory(0xafe2,0x10);
    const auto capturedHigh=DisplayBitmap::captureFrame(frameSource,true);
    if(capturedHigh.width!=640 || capturedHigh.height!=400 ||
       capturedHigh.argb.size()!=640*400 ||
       capturedHigh.argb[0]!=0xff000000u ||
       capturedHigh.argb[1]!=0xffffffffu)return 107;
    const auto emptyMachineFrame=framed.captureDisplayFrame();
    if(emptyMachineFrame.width!=640 || emptyMachineFrame.height!=200 ||
       emptyMachineFrame.argb[0]!=0xff000000u)return 108;
    return 0;
}
