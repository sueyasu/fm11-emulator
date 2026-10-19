#include "Main6809Board.h"
#include <vector>
int main() {
 Main6809Board b; b.reset();
 std::vector<BYTE> image(40*2*16*256);
 const std::size_t off=((1*2+0)*16+0)*256;
 for(int i=0;i<256;++i)image[off+i]=static_cast<BYTE>(i^0x5a);
 b.loadFloppy2d(image);
 // A 2D disk does not make the separate 2HD controller ready.
 // The reference controller's D2SNS is a separate C0 | RDY mask.
 if(b.readCpu(0xfd36)!=0xc1)return 12;
 b.writeCpu(0xfd30,0xd0); // Force interrupt clears FDC status.
 if(b.readCpu(0xfd30)!=0)return 13;
 b.writeCpu(0xfd30,0x08); // RESTORE without verify is valid without 2HD.
 if(b.readCpu(0xfd30)!=0 || b.readCpu(0xfd31)!=0)return 16;
 b.writeCpu(0xfd30,0x0c); // A 2D disk is present in drive 0.
 if((b.readCpu(0xfd30)&0x80)!=0)return 17;
 b.writeCpu(0xfd35,0x01); // Changing drive acknowledges status.
 if(b.readCpu(0xfd30)!=0)return 18;
 b.writeCpu(0xfd30,0x0c);
 b.writeCpu(0xfd34,0x01); // Changing side also clears status.
 if(b.readCpu(0xfd30)!=0)return 19;

 { const auto ipl=b.floppyBootSector(1);
   for(unsigned i=0;i<256;++i)if(ipl[i]!=image[i])return 7; }

 // WD179x Type-I commands must restore/seek the physical track register.
 b.writeCpu(0xfd19,9);
 b.writeCpu(0xfd18,0x00);
 if((b.readCpu(0xfd1f)&0x40)==0)return 14;
 if(b.readCpu(0xfd19)!=0 || (b.readCpu(0xfd18)&0x84)!=0x04)return 8;
 bool sawIndex=false,sawGap=false;
 for(unsigned n=0;n<512;++n) {
   const BYTE status=b.readCpu(0xfd18);
   sawIndex|=(status&0x02)!=0;
   sawGap|=(status&0x02)==0;
 }
 if(!sawIndex || !sawGap)return 10;
 b.writeCpu(0xfd18,0xd0);
 if(b.readCpu(0xfd18)!=0 || (b.readCpu(0xfd1f)&0x40)==0)return 15;
 b.writeCpu(0xfd1b,1);
 b.writeCpu(0xfd18,0x10);
 if(b.readCpu(0xfd19)!=1 || (b.readCpu(0xfd18)&0x04)!=0)return 9;
 // Boot ROM uses the 2HD controller for drive 0, even on a 2D disk.
 b.writeCpu(0xfd35,0);
 b.writeCpu(0xfd34,0);
 b.writeCpu(0xfd31,1);
 b.writeCpu(0xfd32,1);
 b.writeCpu(0xfd44,0x20); b.writeCpu(0xfd45,0);
 b.writeCpu(0xfd46,0x01); b.writeCpu(0xfd47,0);
 b.writeCpu(0xfd51,0x04); // DMA read, channel 1
 b.writeCpu(0xfd30,0x80);
 if((b.readCpu(0xfd51)&0x80)==0)return 20;
 for(int i=0;i<256;++i)
   if(b.readCpu(0x2000+i)!=static_cast<BYTE>(i^0x5a))return 21;
 // DMA count may end before the physical sector ends.
 b.writeCpu(0xfd35,0); b.writeCpu(0xfd34,0);
 b.writeCpu(0xfd31,1); b.writeCpu(0xfd32,1);
 b.writeCpu(0xfd44,0x30); b.writeCpu(0xfd45,0);
 b.writeCpu(0xfd46,0); b.writeCpu(0xfd47,128);
 b.writeCpu(0xfd51,0x04);
 b.writeCpu(0xfd30,0x80);
 if((b.readCpu(0xfd51)&0x80)==0)return 22;
 for(int i=0;i<128;++i)
   if(b.readCpu(0x3000+i)!=static_cast<BYTE>(i^0x5a))return 23;
 b.writeCpu(0xfd19,1); b.writeCpu(0xfd1c,0);
 b.writeCpu(0xfd1a,1); b.writeCpu(0xfd1d,0);
 b.writeCpu(0xfd50,0x00);
 b.writeCpu(0xfd18,0x80);
 if((b.readCpu(0xfd18)&0x02)==0)return 1;
 for(int i=0;i<256;++i)
   if(b.readCpu(0xfd1b)!=static_cast<BYTE>(i^0x5a))return 2;
 if(b.readCpu(0xfd18)!=0)return 3;
 b.writeCpu(0xfd40,0x20);b.writeCpu(0xfd41,0);
 b.writeCpu(0xfd42,1);b.writeCpu(0xfd43,0);
 b.writeCpu(0xfd50,0x04);b.writeCpu(0xfd18,0x80);
 if((b.readCpu(0xfd50)&0x80)==0)return 4;
 for(int i=0;i<256;++i)
   if(b.readCpu(0x2000+i)!=static_cast<BYTE>(i^0x5a))return 5;
 // ROM loads sectors 1-4 from physical 2HD drive 3, but the IPL
 // subsequently selects MiniFDC drive 0 to read sector 5 onward.
 b.setFloppyDrive(3);
 b.writeCpu(0xfd35,3); b.writeCpu(0xfd34,0);
 b.writeCpu(0xfd31,1); b.writeCpu(0xfd32,1);
 b.writeCpu(0xfd44,0x31); b.writeCpu(0xfd45,0);
 b.writeCpu(0xfd46,1); b.writeCpu(0xfd47,0);
 b.writeCpu(0xfd51,0x04); b.writeCpu(0xfd30,0x80);
 if((b.readCpu(0xfd51)&0x80)==0)return 24;
 for(int i=0;i<256;++i)
   if(b.readCpu(0x3100+i)!=static_cast<BYTE>(i^0x5a))return 25;
 b.writeCpu(0xfd19,1); b.writeCpu(0xfd1a,1);
 b.writeCpu(0xfd1c,0); b.writeCpu(0xfd1d,0);
 b.writeCpu(0xfd50,0);
 b.writeCpu(0xfd18,0x80);
 if((b.readCpu(0xfd18)&0x10)!=0)return 26;
 for(int i=0;i<256;++i)
   if(b.readCpu(0xfd1b)!=static_cast<BYTE>(i^0x5a))return 27;
 b.setFloppyDrive(0);
 b.writeCpu(0xfd1a,17);b.writeCpu(0xfd18,0x80);
 if((b.readCpu(0xfd18)&0x10)==0)return 6;
 return 0;
}
