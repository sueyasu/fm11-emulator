#include "Main6809Board.h"

#include <stdexcept>

Main6809Board::Main6809Board(CpuType variant)
    : core_(memory_, variant)
{
    if (variant != CpuType::MC6809 && variant != CpuType::HD6309)
        throw std::invalid_argument("Main6809Board requires MC6809 or HD6309 CPU type");
}

std::string_view Main6809Board::name() const
{
    return "main6809";
}

CpuCore& Main6809Board::cpu()
{
    return core_;
}

const CpuCore& Main6809Board::cpu() const
{
    return core_;
}

void Main6809Board::reset()
{
    core_.reset();
}

BYTE Main6809Board::readPhysical(WORD address) const
{
    return memory_[address];
}

void Main6809Board::writePhysical(WORD address, BYTE value)
{
    memory_[address] = value;
}

Main6809Board::Core::Core(std::array<BYTE, 65536>& memory, CpuType variant)
    : memory_(memory), variant_(variant)
{
}

BYTE Main6809Board::Core::GetMemByte(WORD address)
{
    return memory_[address];
}

void Main6809Board::Core::SetMemByte(WORD address, BYTE value)
{
    memory_[address] = value;
}

void Main6809Board::Core::reset()
{
    SimRESET();
}

void Main6809Board::Core::executeOne()
{
    SimONE();
}

std::uint32_t Main6809Board::Core::pc() const
{
    return regs.r.w.PC;
}

void Main6809Board::Core::irq()
{
    SimIRQ();
}

void Main6809Board::Core::nmi()
{
    SimNMI();
}

void Main6809Board::Core::firq()
{
    SimFIRQ();
}

CpuType Main6809Board::Core::type() const
{
    return variant_;
}
