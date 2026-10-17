#include "Sim09.h"
#include <array>
class RamCpu : public CSim09 {
public:
 std::array<BYTE,65536> data{};
 BYTE GetMemByte(WORD a) override {return data[a];}
 void SetMemByte(WORD a,BYTE v) override {data[a]=v;}
};
int main() {
 // 6309-only immediate-memory opcode, basic 6809 JMP, 6309 page 10 ADDR
 // and standard page 11 CMPU, with all three personalities exercised.
 const BYTE code[]={0x01,0xff,0x80,0x10,0x21,0,0,0x11,0x83,0,0,0x12};
 for (int mode=0;mode<2;++mode) {
   RamCpu cpu;cpu.Set6309Enabled(true);
   cpu.data[0xfffe]=0x10;cpu.data[0xffff]=0;
   for(unsigned i=0;i<sizeof(code);++i)cpu.data[0x1000+i]=code[i];
   cpu.SimRESET();if(mode)cpu.regs.MD=1;
   for(int i=0;i<4;++i) {
     cpu.SimONE();
     if(cpu.LastInstructionCycles()==0)return 1+i+mode*4;
   }
   if(cpu.regs.r.w.PC!=0x1000+sizeof(code))return 10+mode;
 }
 return 0;
}
