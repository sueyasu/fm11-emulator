#include "Main6809Board.h"

namespace {

void prepare(Main6809Board& board, WORD pc)
{
    Main6809Board::BootRomImage bootRom{};
    bootRom[0x0ffe] = static_cast<BYTE>(pc >> 8);
    bootRom[0x0fff] = static_cast<BYTE>(pc);
    board.loadBootRom(bootRom);
    board.reset();

    // The illegal-opcode vector lives in fixed on-board RAM, not physical RAM.
    board.writeCpu(0xfff0, 0x40);
    board.writeCpu(0xfff1, 0x00);
}

bool rejectsBase6309Opcode()
{
    Main6809Board board(CpuType::MC6809);
    prepare(board, 0x1000);
    board.writePhysical(0x1000, 0x14); // SEXW, HD6309 only
    board.cpu().executeOne();
    return board.cpu().pc() == 0x4000;
}

bool acceptsBase6309OpcodeOn6309()
{
    Main6809Board board(CpuType::HD6309);
    prepare(board, 0x1000);
    board.writePhysical(0x1000, 0x14); // SEXW
    board.cpu().executeOne();
    return board.cpu().pc() == 0x1001;
}

bool rejectsPage10ExtendedOpcode()
{
    Main6809Board board(CpuType::MC6809);
    prepare(board, 0x1000);
    board.writePhysical(0x1000, 0x10);
    board.writePhysical(0x1001, 0x30); // ADDR, HD6309 only
    board.writePhysical(0x1002, 0x89);
    board.cpu().executeOne();
    return board.cpu().pc() == 0x4000;
}

bool rejectsPage11ExtendedOpcode()
{
    Main6809Board board(CpuType::MC6809);
    prepare(board, 0x1000);
    board.writePhysical(0x1000, 0x11);
    board.writePhysical(0x1001, 0x3c); // BITMD, HD6309 only
    board.writePhysical(0x1002, 0x00);
    board.cpu().executeOne();
    return board.cpu().pc() == 0x4000;
}

bool acceptsStandardPage10OpcodeOn6809()
{
    Main6809Board board(CpuType::MC6809);
    prepare(board, 0x1000);
    board.writePhysical(0x1000, 0x10);
    board.writePhysical(0x1001, 0x83); // CMPD immediate, MC6809 instruction
    board.writePhysical(0x1002, 0x00);
    board.writePhysical(0x1003, 0x00);
    board.cpu().executeOne();
    return board.cpu().pc() == 0x1004;
}

bool acceptsStandardPage11OpcodeOn6809()
{
    Main6809Board board(CpuType::MC6809);
    prepare(board, 0x1000);
    board.writePhysical(0x1000, 0x11);
    board.writePhysical(0x1001, 0x83); // CMPU immediate, MC6809 instruction
    board.writePhysical(0x1002, 0x00);
    board.writePhysical(0x1003, 0x00);
    board.cpu().executeOne();
    return board.cpu().pc() == 0x1004;
}

bool rejects6309AccumulatorIndexedOn6809()
{
    Main6809Board board(CpuType::MC6809);
    prepare(board, 0x1000);
    board.writePhysical(0x1000, 0xa6); // LDA indexed
    board.writePhysical(0x1001, 0x87); // E,X -- HD6309 only
    board.cpu().executeOne();
    return board.cpu().pc() == 0x4000;
}

bool accepts6309AccumulatorIndexedOn6309()
{
    Main6809Board board(CpuType::HD6309);
    prepare(board, 0x1000);
    board.writePhysical(0x1000, 0xa6); // LDA indexed
    board.writePhysical(0x1001, 0x87); // E,X
    board.cpu().executeOne();
    return board.cpu().pc() == 0x1002;
}

bool rejects6309WIndexedOn6809()
{
    Main6809Board board(CpuType::MC6809);
    prepare(board, 0x1000);
    board.writePhysical(0x1000, 0xa6); // LDA indexed
    board.writePhysical(0x1001, 0x8f); // ,W -- HD6309 only
    board.cpu().executeOne();
    return board.cpu().pc() == 0x4000;
}

bool acceptsStandardIndexedOn6809()
{
    Main6809Board board(CpuType::MC6809);
    prepare(board, 0x1000);
    board.writePhysical(0x1000, 0xa6); // LDA indexed
    board.writePhysical(0x1001, 0x84); // ,X -- MC6809
    board.cpu().executeOne();
    return board.cpu().pc() == 0x1002;
}

bool rejects6309TfrRegisterOn6809()
{
    Main6809Board board(CpuType::MC6809);
    prepare(board, 0x1000);
    board.writePhysical(0x1000, 0x1f); // TFR
    board.writePhysical(0x1001, 0xe8); // E,A -- E is HD6309 only
    board.cpu().executeOne();
    return board.cpu().pc() == 0x4000;
}

bool accepts6309TfrRegisterOn6309()
{
    Main6809Board board(CpuType::HD6309);
    prepare(board, 0x1000);
    board.writePhysical(0x1000, 0x1f); // TFR
    board.writePhysical(0x1001, 0xe8); // E,A
    board.cpu().executeOne();
    return board.cpu().pc() == 0x1002;
}

bool rejects6309ExgRegisterOn6809()
{
    Main6809Board board(CpuType::MC6809);
    prepare(board, 0x1000);
    board.writePhysical(0x1000, 0x1e); // EXG
    board.writePhysical(0x1001, 0xef); // E,F -- HD6309 only
    board.cpu().executeOne();
    return board.cpu().pc() == 0x4000;
}

bool acceptsStandardTfrOn6809()
{
    Main6809Board board(CpuType::MC6809);
    prepare(board, 0x1000);
    board.writePhysical(0x1000, 0x1f); // TFR
    board.writePhysical(0x1001, 0x89); // A,B -- MC6809
    board.cpu().executeOne();
    return board.cpu().pc() == 0x1002;
}

} // namespace

int main()
{
    if (!rejectsBase6309Opcode())
        return 1;
    if (!acceptsBase6309OpcodeOn6309())
        return 2;
    if (!rejectsPage10ExtendedOpcode())
        return 3;
    if (!rejectsPage11ExtendedOpcode())
        return 4;
    if (!acceptsStandardPage10OpcodeOn6809())
        return 5;
    if (!acceptsStandardPage11OpcodeOn6809())
        return 6;
    if (!rejects6309AccumulatorIndexedOn6809())
        return 7;
    if (!accepts6309AccumulatorIndexedOn6309())
        return 8;
    if (!rejects6309WIndexedOn6809())
        return 9;
    if (!acceptsStandardIndexedOn6809())
        return 10;
    if (!rejects6309TfrRegisterOn6809())
        return 11;
    if (!accepts6309TfrRegisterOn6309())
        return 12;
    if (!rejects6309ExgRegisterOn6809())
        return 13;
    if (!acceptsStandardTfrOn6809())
        return 14;
    return 0;
}
