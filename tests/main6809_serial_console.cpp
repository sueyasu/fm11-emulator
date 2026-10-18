#include "Main6809Board.h"

int main()
{
    Main6809Board b(CpuType::MC6809);
    b.reset();
    if (b.readCpu(0xfd07) != 0x81) return 1;
    // The serial interrupt is masked by FD02 until the guest enables it.
    b.writeCpu(0xfd07, 0x04); // Rx enable, Tx disabled
    b.queueSerialInput('O');
    b.queueSerialInput('K');
    if (b.irqPending()) return 2;
    if ((b.readCpu(0xfd07) & 0x02) == 0) return 3;
    b.writeCpu(0xfd02, 0x04);
    if (!b.irqPending() || (b.readCpu(0xfd03) & 0x80) == 0) return 4;
    if (b.readCpu(0xfd06) != 'O') return 5;
    if (!b.irqPending()) return 6;
    if (b.readCpu(0xfd06) != 'K') return 7;
    if (b.irqPending() || (b.readCpu(0xfd07) & 0x02) != 0) return 8;
    b.writeCpu(0xfd07, 0x01); // Tx enable
    if (!b.irqPending()) return 9;
    b.writeCpu(0xfd06, 'H'); b.writeCpu(0xfd06, 'i');
    BYTE c=0;
    if (!b.takeSerialOutput(c) || c!='H') return 10;
    if (!b.takeSerialOutput(c) || c!='i') return 11;
    if (b.takeSerialOutput(c)) return 12;
    // Diagnostic switch removes only the Tx IRQ, not TX output or RX IRQ.
    b.setTxIrqEnabled(false);
    if (b.irqPending()) return 15;
    b.writeCpu(0xfd06, 'T');
    if (!b.takeSerialOutput(c) || c!='T') return 16;
    b.writeCpu(0xfd07, 0x05); // Tx and Rx both enabled
    b.queueSerialInput('Q');
    if (!b.irqPending() || b.readCpu(0xfd06)!='Q') return 17;
    if (b.irqPending()) return 18;
    b.setTxIrqEnabled(true);
    if (!b.irqPending()) return 19;
    b.writeCpu(0xfd07, 0x00);
    if (b.irqPending()) return 13;
    b.queueSerialInput('R');
    b.reset();
    if (b.readCpu(0xfd07)!=0x81 || b.readCpu(0xfd06)!=0) return 14;
    return 0;
}
