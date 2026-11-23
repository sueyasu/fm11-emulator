#include "Usart8251.h"
#include "Fm11SerialClock.h"
#include <cstdint>

int main() {
    using namespace Fm11SerialClock;
    if(lookup(0).asyncBaud!=300 || lookup(4).asyncBaud!=4800 ||
       lookup(5).asyncBaud!=9600 || lookup(4).fastBaud!=19200 ||
       lookup(8).asyncBaud!=75 || lookup(9).asyncBaud!=150 ||
       lookup(6).divider!=0 || lookup(31).asyncBaud!=0 ||
       lookup(32).divider!=0 || lookup(26).divider!=4096)return 1;
    Usart8251 u;
    u.reset();
    u.setBaudRate(1000);
    u.writeCommand(0x4e); // asynchronous, x16, 8N1: 10 bits per frame
    u.writeCommand(0x05); // enable Tx and Rx
    if((u.status()&0x07)!=0x05)return 2;
    u.writeData('A');
    if((u.status()&0x07)!=0x01)return 3; // ready for second byte, shift busy
    u.writeData('B');
    if((u.status()&0x07)!=0x00)return 4; // holding buffer full
    u.advanceNanoseconds(9999999);
    BYTE out=0;
    if(u.takeOutput(out) || (u.status()&0x01))return 5;
    u.advanceNanoseconds(1);
    if(!u.takeOutput(out) || out!='A' ||
       (u.status()&0x07)!=0x01)return 6; // B shifted to wire
    u.advanceNanoseconds(10000000);
    if(!u.takeOutput(out) || out!='B' || u.takeOutput(out) ||
       (u.status()&0x07)!=0x05)return 7;
    u.receive('X');
    u.advanceNanoseconds(9999999);
    if(u.rxReady())return 8;
    u.advanceNanoseconds(1);
    if(!u.rxReady() || u.readData()!='X' || u.rxReady())return 9;
    // Overrun overwrites unread character, and ER clears the sticky flag.
    u.receive('Y');u.receive('Z');
    u.advanceNanoseconds(20000000);
    if(!u.rxReady() || !(u.status()&0x10) || u.readData()!='Z')return 10;
    u.writeCommand(0x15); // ER + TxEN/RxEN
    if(u.status()&0x38)return 11;
    u.writeCommand(0x40); // internal reset preserves external baud
    if(u.phase()!=Usart8251::ControlPhase::Mode || u.baudRate()!=1000 ||
       (u.status()&0x07)!=0x04)return 12;
    // Default remains byte-complete so PTY and OS startup do not regress.
    Usart8251 immediate;
    immediate.reset();
    immediate.writeCommand(0x4e);immediate.writeCommand(0x05);
    immediate.writeData('Q');
    if(!immediate.takeOutput(out) || out!='Q')return 13;
    immediate.receive('R');
    if(!immediate.rxReady() || immediate.readData()!='R')return 14;
    return 0;
}
