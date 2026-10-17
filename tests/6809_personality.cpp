#include "Main6809Board.h"

namespace {

void prepare(Main6809Board& board, WORD pc)
{
    board.writePhysical(0xfffe, static_cast<BYTE>(pc >> 8));
    board.writePhysical(0xffff, static_cast<BYTE>(pc));
    board.writePhysical(0xfff0, 0x40);
    board.writePhysical(0xfff1, 0x00);
    board.reset();
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
    return 0;
}
