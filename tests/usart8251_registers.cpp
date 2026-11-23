#include "Usart8251.h"
#include <cstdint>

int main() {
    Usart8251 u;
    u.reset();
    if(u.status()!=0x84 || u.irqRequest() ||
       u.phase()!=Usart8251::ControlPhase::Mode)return 1;
    // First control write is a MODE, not a command. Both CPUs share it.
    u.writeCommand(0x4e);
    if(u.mode()!=0x4e || u.phase()!=Usart8251::ControlPhase::Command ||
       (u.status()&0x01))return 2;
    u.writeCommand(0x27); // TxEN, RxEN, DTR, RTS
    if(!u.dtr() || !u.rts() || !u.cts() || !u.dsr() || !u.dcd())return 24;
    if(!u.txReady() || !u.irqRequest() || (u.status()&0x87)!=0x85)return 3;
    u.writeData('A');
    if((u.status()&0x04)!=0)return 4;
    BYTE ch=0;
    if(!u.takeOutput(ch) || ch!='A' || (u.status()&0x04)==0)return 5;
    u.receive('X');u.receive('Y'); // host burst must not discard characters
    if(!u.rxReady() || u.readData()!='X' || !u.rxReady() ||
       u.readData()!='Y' || u.rxReady())return 6;
    u.setTxIrqEnabled(false);
    if(u.irqRequest())return 7;
    u.receive('Z');
    if(!u.irqRequest() || u.readData()!='Z' || u.irqRequest())return 8;
    u.setTxIrqEnabled(true);
    u.setModemInputs(false,false,false);
    if(u.cts() || u.dsr() || u.dcd() || !u.dtr() || !u.rts())return 25;
    if(u.txReady() || u.irqRequest() || (u.status()&0x80))return 9;
    u.setModemInputs(true,true);
    if(!u.cts() || !u.dsr() || !u.dcd())return 26;
    if(!u.txReady() || !u.irqRequest())return 10;
    u.signalReceiveError(true,true,true);
    if((u.status()&0x38)!=0x38)return 11;
    u.writeCommand(0x37); // command + ER pulse
    if((u.status()&0x38)!=0 || (u.command()&0x10)!=0)return 12;
    u.writeCommand(0x2f); // send break
    if(u.txReady())return 13;
    u.writeData('B');
    if(u.takeOutput(ch))return 14;
    u.writeCommand(0x27);
    if(!u.txReady())return 15;
    u.writeCommand(0x40); // internal reset
    if(u.dtr() || u.rts())return 27;
    if(u.phase()!=Usart8251::ControlPhase::Mode || u.irqRequest() ||
       (u.status()&0x87)!=0x84)return 16;
    // RTS and DTR follow the command independently of TxEN/RxEN.
    u.writeCommand(0x4e);
    u.writeCommand(0x22); // DTR and RTS only
    if(!u.dtr() || !u.rts() || u.txReady() || u.rxReady())return 28;
    u.writeCommand(0x00);
    if(u.dtr() || u.rts())return 29;
    u.writeCommand(0x40); // return to mode phase
    // Synchronous mode with two sync characters.
    u.writeCommand(0x00);
    if(u.phase()!=Usart8251::ControlPhase::Sync1)return 17;
    u.writeCommand(0x16);
    if(u.phase()!=Usart8251::ControlPhase::Sync2)return 18;
    u.writeCommand(0x2a);
    if(u.phase()!=Usart8251::ControlPhase::Command ||
       u.sync1()!=0x16 || u.sync2()!=0x2a)return 19;
    u.writeCommand(0x45); // internal reset wins over RxEN
    if(u.phase()!=Usart8251::ControlPhase::Mode)return 20;
    // Synchronous mode with one sync character (mode bit 7).
    u.writeCommand(0x80);
    u.writeCommand(0xa5);
    if(u.phase()!=Usart8251::ControlPhase::Command || u.sync1()!=0xa5)return 21;
    u.setSyncDetected(true);
    if(!(u.status()&0x40))return 22;
    u.writeCommand(0x80); // enter hunt
    if(u.status()&0x40)return 23;
    return 0;
}
