#include "Main8088Bus.h"
#include <memory>
#include <vector>
#include <cstdio>

// Reproduces the stock FM-11 8088 boot ROM's 2HD polling contract:
// FF00:074A issues READ SECTOR, FF00:0684 waits for FD37 bit 6,
// FF00:07C6 requires FD30=00 before accepting the boot sector.
// In particular, FD37 may not float high while Type-II is BUSY.
int main() {
    Main8088Bus bus;
    auto fdc=std::make_shared<FloppyDevice>();
    bus.setFloppyDevice(fdc);
    std::vector<BYTE> disk(77u*2*26*256,0);
    disk[0]=0x5a;
    disk[2u*26*256]=0xa5; // C1/H0/S1
    fdc->load2hd(disk);
    bus.reset();
    fdc->enableSeekTiming(true);
    bus.writeIo(0xfd35,0);
    bus.writeIo(0xfd34,0);
    bus.writeIo(0xfd31,0);
    bus.writeIo(0xfd32,1);

    bus.writeIo(0xfd30,0x08); // RESTORE (Type I)
    if((bus.readIo(0xfd30)&1)==0 || (bus.readIo(0xfd37)&0x40))return 1;
    fdc->advanceSeekNanoseconds(30000000);
    if((bus.readIo(0xfd37)&0x40)==0 || (bus.readIo(0xfd30)&4)==0)return 2;

    // 8237 channel 1, 256 bytes, device -> memory
    auto setupDma=[&](WORD dst) {
        bus.writeIo(0x1c,0);
        bus.writeIo(0x12,BYTE(dst));
        bus.writeIo(0x12,BYTE(dst>>8));
        bus.writeIo(0x13,0xff);
        bus.writeIo(0x13,0x00);
        bus.writeIo(0x1b,0x45);
        bus.writeIo(0x1a,1);
    };
    setupDma(0x4000);
    bus.writeIo(0xfd30,0x80);
    // ROM polls FD37; the prior RESTORE IRQ must be cleared here.
    if((bus.readIo(0xfd30)&1)==0 || (bus.readIo(0xfd37)&0x40))return 3;
    // Sector 1 passes after ~139 ms from the post-RESTORE angle.
    // DMA terminal count is reached before the FDC completion interrupt.
    fdc->advanceSeekNanoseconds(140000000);
    bus.serviceTimedFloppy();
    const BYTE irqAtArrival=bus.readIo(0xfd37);
    const BYTE statusAtArrival=bus.readIo(0xfd30);
    const BYTE dmaByteAtArrival=bus.readRam(0x4000);
    if((irqAtArrival&0x40) || (statusAtArrival&1)==0 ||
       dmaByteAtArrival!=0x5a) {
        std::fprintf(stderr,
            "Type-II arrival: FD37=%02X FD30=%02X DMA[4000]=%02X "
            "pending=%d ready=%d transfer=%d DMAcount=%04X\\n",
            unsigned(irqAtArrival),unsigned(statusAtArrival),
            unsigned(dmaByteAtArrival),int(fdc->typeIIBusy(1,0)),
            int(fdc->typeIIReady()),int(fdc->typeIITransferring(1,0)),
            unsigned(bus.dmaChannel(1).count));
        return 9;
    }
    fdc->advanceSeekNanoseconds(20000000);
    bus.serviceTimedFloppy();
    // A prior Type-I TRACK0 (04h) must not leak into the Type-II result.
    if((bus.readIo(0xfd37)&0x40)==0 || bus.readIo(0xfd30)!=0 ||
       bus.readRam(0x4000)!=0x5a || bus.dmaChannel(1).count!=0xffff)
        return 4;

    // Same sequence after SEEK, which also leaves a Type-I completion.
    bus.writeIo(0xfd33,1);
    bus.writeIo(0xfd30,0x18);
    if((bus.readIo(0xfd37)&0x40))return 5;
    fdc->advanceSeekNanoseconds(50000000);
    if((bus.readIo(0xfd37)&0x40)==0 || bus.readIo(0xfd31)!=1)return 6;
    bus.writeIo(0xfd32,1);
    setupDma(0x4100);
    bus.writeIo(0xfd30,0x80);
    if((bus.readIo(0xfd37)&0x40) || (bus.readIo(0xfd30)&1)==0)return 7;
    fdc->advanceSeekNanoseconds(200000000);
    bus.serviceTimedFloppy();
    if((bus.readIo(0xfd37)&0x40)==0 || bus.readIo(0xfd30)!=0 ||
       bus.readRam(0x4100)!=0xa5)return 8;
    return 0;
}
