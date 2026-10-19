#include "Main6809Board.h"
#include <vector>
int main() {
 Main6809Board b; b.reset();
 std::vector<BYTE> image(40*2*16*256);
 const std::size_t off=((1*2+0)*16+0)*256;
 for(int i=0;i<256;++i)image[off+i]=static_cast<BYTE>(i^0x5a);
 b.loadFloppy2d(image);
 { const auto ipl=b.floppyBootSector(1);
   for(unsigned i=0;i<256;++i)if(ipl[i]!=image[i])return 7; }

 // WD179x Type-I commands must restore/seek the physical track register.
 b.writeCpu(0xfd19,9);
 b.writeCpu(0xfd18,0x00);
 if(b.readCpu(0xfd19)!=0 || (b.readCpu(0xfd18)&0x80)!=0)return 8;
 b.writeCpu(0xfd1b,1);
 b.writeCpu(0xfd18,0x10);
 if(b.readCpu(0xfd19)!=1)return 9;
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
 b.writeCpu(0xfd1a,17);b.writeCpu(0xfd18,0x80);
 if((b.readCpu(0xfd18)&0x10)==0)return 6;
 return 0;
}
