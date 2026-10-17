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
    ioOutputLatch_.fill(0);
    ioStatusLatch_.fill(0);
    mainIrqSources_ = extendedIrqSources_ = 0;
    mainIrqResult_ = extendedIrqResult_ = 0;

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

void Main6809Board::signalAttention()
{
    ioRegisters_[0x04] |= 0x01;
}

void Main6809Board::signalBreak()
{
    ioRegisters_[0x04] |= 0x02;
}

void Main6809Board::setMainIrqSource(BYTE sourceBit, bool asserted)
{
    // FD03: RS232C, KEYBOARD, DMA, MINI FDC, PRINTER and PTM.
    if ((sourceBit & static_cast<BYTE>(~0xdb)) != 0)
        throw std::invalid_argument("invalid main IRQ source bit");
    if (asserted)
        mainIrqSources_ |= sourceBit;
    else
        mainIrqSources_ &= static_cast<BYTE>(~sourceBit);
    mainIrqStatus();
}

void Main6809Board::setExtendedIrqSource(BYTE sourceBit, bool asserted)
{
    // FD2C: RS232C, SFD, HDD, LPT2, LPT1.
    if ((sourceBit & static_cast<BYTE>(~0xc7)) != 0)
        throw std::invalid_argument("invalid extended IRQ source bit");
    if (asserted)
        extendedIrqSources_ |= sourceBit;
    else
        extendedIrqSources_ &= static_cast<BYTE>(~sourceBit);
    mainIrqStatus();
}

bool Main6809Board::irqPending() const
{
    return mainIrqResult_ != 0;
}

BYTE Main6809Board::extendedIrqStatus()
{
    // The original Fm11EIRQ combines FY(2C) with FX(2C), except that
    // the HDD interrupt is gated by FD C2 bit 7.
    const BYTE mask = ioOutputLatch_[0x2c];
    BYTE value = static_cast<BYTE>(extendedIrqSources_ & mask & 0xc3);
    if ((extendedIrqSources_ & 0x04) != 0 && (ioRegisters_[0xc2] & 0x80) != 0)
        value |= 0x04;
    extendedIrqResult_ = value;
    ioRegisters_[0x2c] = value;
    return value;
}

BYTE Main6809Board::mainIrqStatus()
{
    const BYTE mask = ioOutputLatch_[0x02];
    BYTE value = 0;
    // Serial input in the original Fm11IRQ uses mask bits 1/2, rather
    // than a direct one-to-one mask with the FD03 status bit.
    if ((mainIrqSources_ & 0x80) != 0 && (mask & 0x06) != 0)
        value |= 0x80;
    if ((mainIrqSources_ & 0x40) != 0 && (mask & 0x80) != 0)
        value |= 0x40;
    if ((mainIrqSources_ & 0x10) != 0 && (ioRegisters_[0x55] & 0x0f) != 0)
        value |= 0x10;
    if ((mainIrqSources_ & 0x08) != 0 && (mask & 0x20) != 0)
        value |= 0x08;
    if ((mainIrqSources_ & 0x02) != 0 && (mask & 0x01) != 0)
        value |= 0x02;
    if ((mainIrqSources_ & 0x01) != 0 && (mask & 0x10) != 0)
        value |= 0x01;
    if (extendedIrqStatus() != 0)
        value |= 0x20;
    mainIrqResult_ = value;
    ioRegisters_[0x03] = value;
    return value;
}

BYTE Main6809Board::readCpu(WORD address)
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

BYTE Main6809Board::readIo(WORD address)
{
    if (isMmrRegister(address))
        return readMmrRegister(address);

    const BYTE reg = static_cast<BYTE>(address & 0x00ff);
    switch (reg) {
    case 0x03:
        return mainIrqStatus();
    case 0x2c: {
        const BYTE value = extendedIrqStatus();
        ioRegisters_[0x2c] = 0; // FD2C read ACK (FY sources remain asserted)
        return value;
    }
    case 0x04: {
        // Reading the main event register acknowledges BREAK and ATENT.
        const BYTE value = ioRegisters_[reg];
        ioRegisters_[reg] &= static_cast<BYTE>(~0x03);
        return value;
    }
    case 0x05:
        // Main/sub CPU control status is CPU-visible through FD05.
        return ioRegisters_[reg];
    default:
        // Device-specific side effects are introduced with each device. Until
        // then, retain a side-effect-free register backing for the FDxx aperture.
        return ioRegisters_[reg];
    }
}

void Main6809Board::writeIo(WORD address, BYTE value)
{
    if (isMmrRegister(address)) {
        writeMmrRegister(address, value);
        return;
    }

    const BYTE reg = static_cast<BYTE>(address & 0x00ff);
    switch (reg) {
    case 0x02:
        ioOutputLatch_[0x02] = value;
        mainIrqStatus();
        break;
    case 0x2c:
        ioOutputLatch_[0x2c] = value;
        mainIrqStatus();
        break;
    case 0x03:
        // FD03 is a read-only IRQ status register (write controls beep).
        break;
    case 0x04:
        // The original ATENT ACK write clears ATENT and records an 0x80
        // control latch. BREAK is only acknowledged by reading FD04.
        ioRegisters_[reg] &= static_cast<BYTE>(~0x01);
        ioOutputLatch_[reg] = 0x80;
        break;
    case 0x05:
        // FD05 writes go to the output/control latch. A HALT request sets the
        // CPU-visible halt status and clears the corresponding input status.
        ioOutputLatch_[reg] = value;
        if ((value & 0x80) != 0) {
            ioRegisters_[reg] |= 0x80;
            ioStatusLatch_[reg] &= 0x7f;
        }
        break;
    default:
        ioRegisters_[reg] = value;
        if (reg == 0x55 || reg == 0xc2)
            mainIrqStatus();
        break;
    }
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
