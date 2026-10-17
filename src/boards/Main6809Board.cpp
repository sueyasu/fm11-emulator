#include "Main6809Board.h"

#include <stdexcept>

Main6809Board::Main6809Board(CpuType variant)
    : memory_(kPhysicalMemorySize, 0), core_(*this, variant)
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
    for (auto& page : mmr_)
        page.fill(0);
    mmrControl_.fill(0);
    onboardRam_.fill(0);
    ioRegisters_.fill(0);

    // The original board copies the reset vector from the last two bytes of
    // the boot ROM into the fixed on-board RAM at FFFE/FFFF before RESET.
    onboardRam_[0x0ffe] = bootRom_[0x0ffe];
    onboardRam_[0x0fff] = bootRom_[0x0fff];
    core_.reset();
}

void Main6809Board::loadBootRom(const BootRomImage& image)
{
    bootRom_ = image;
}

BYTE Main6809Board::readCpu(WORD address) const
{
    // 5000-5FFF is the 4 KiB boot ROM while FD93 bit 0 is clear.
    if (address >= 0x5000 && address <= 0x5fff && (mmrControl_[3] & 0x01) == 0)
        return bootRom_[address & 0x0fff];

    // 7C00-7FFF can be redirected by the window registers FD91/FD92.
    if (address >= 0x7c00 && address <= 0x7fff && (mmrControl_[3] & 0x40) != 0)
        return readPhysical(windowPhysicalAddress(address));

    // FC00-FFFF is fixed on-board space. FD00-FDFF is I/O; the remaining
    // three 256-byte regions are backed by the board's 4 KiB local RAM.
    if (address >= 0xfc00) {
        if ((address & 0xff00) == 0xfd00)
            return readIo(address);
        return onboardRam_[address & 0x0fff];
    }

    return readPhysical(translateCpuAddress(address));
}

void Main6809Board::writeCpu(WORD address, BYTE value)
{
    // Writes are ignored while the boot ROM is visible.
    if (address >= 0x5000 && address <= 0x5fff && (mmrControl_[3] & 0x01) == 0)
        return;

    if (address >= 0x7c00 && address <= 0x7fff && (mmrControl_[3] & 0x40) != 0) {
        writePhysical(windowPhysicalAddress(address), value);
        return;
    }

    if (address >= 0xfc00) {
        if ((address & 0xff00) == 0xfd00)
            writeIo(address, value);
        else
            onboardRam_[address & 0x0fff] = value;
        return;
    }

    writePhysical(translateCpuAddress(address), value);
}

BYTE Main6809Board::readPhysical(std::uint32_t address) const
{
    if (address >= memory_.size())
        throw std::out_of_range("Main6809Board physical read outside backing memory");
    return memory_[address];
}

void Main6809Board::writePhysical(std::uint32_t address, BYTE value)
{
    if (address >= memory_.size())
        throw std::out_of_range("Main6809Board physical write outside backing memory");
    memory_[address] = value;
}

bool Main6809Board::isMmrRegister(WORD address) const
{
    return address >= 0xfd80 && address <= 0xfd93;
}

BYTE Main6809Board::readMmrRegister(WORD address) const
{
    if (address >= 0xfd80 && address <= 0xfd8f) {
        const auto logicalPage = static_cast<std::size_t>(address & 0x000f);
        const auto context = static_cast<std::size_t>(mmrControl_[0] & 0x0f);
        return mmr_[logicalPage][context];
    }

    return mmrControl_[address - 0xfd90];
}

void Main6809Board::writeMmrRegister(WORD address, BYTE value)
{
    if (address >= 0xfd80 && address <= 0xfd8f) {
        const auto logicalPage = static_cast<std::size_t>(address & 0x000f);
        const auto context = static_cast<std::size_t>(mmrControl_[0] & 0x0f);
        mmr_[logicalPage][context] = value;
        return;
    }

    const auto index = static_cast<std::size_t>(address - 0xfd90);
    mmrControl_[index] = (index <= 1) ? static_cast<BYTE>(value & 0x0f) : value;
}

BYTE Main6809Board::readIo(WORD address) const
{
    if (isMmrRegister(address))
        return readMmrRegister(address);

    // Device-specific side effects are introduced with each device. Until
    // then, retain a side-effect-free register backing for the FDxx aperture.
    return ioRegisters_[address & 0x00ff];
}

void Main6809Board::writeIo(WORD address, BYTE value)
{
    if (isMmrRegister(address)) {
        writeMmrRegister(address, value);
        return;
    }

    ioRegisters_[address & 0x00ff] = value;
}

std::uint32_t Main6809Board::translateCpuAddress(WORD address) const
{
    if ((mmrControl_[3] & 0x80) == 0)
        return address;

    const auto logicalPage = static_cast<std::size_t>(address >> 12);
    const auto context = static_cast<std::size_t>(mmrControl_[0] & 0x0f);
    const std::uint32_t physicalPage = mmr_[logicalPage][context];
    return (physicalPage << 12) | (address & (kMmrPageSize - 1));
}

std::uint32_t Main6809Board::windowPhysicalAddress(WORD address) const
{
    const std::uint32_t bank = static_cast<std::uint32_t>(mmrControl_[1] & 0x0f) << 16;
    const WORD low = static_cast<WORD>((static_cast<WORD>(mmrControl_[2]) << 8) + address);
    return bank | low;
}

Main6809Board::Core::Core(Main6809Board& board, CpuType variant)
    : board_(board), variant_(variant)
{
    Set6309Enabled(variant_ == CpuType::HD6309);
}

BYTE Main6809Board::Core::GetMemByte(WORD address)
{
    return board_.readCpu(address);
}

void Main6809Board::Core::SetMemByte(WORD address, BYTE value)
{
    board_.writeCpu(address, value);
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
