#include "Main6809Board.h"

#include <stdexcept>
#include <algorithm>

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
    if(mainBoard_)
        mainBoard_->setIrqLine(MainBoard::IrqLine::Timer,false);
    usart_->reset();
    ptm_.fill(PtmChannel{});
    ptmStatus_ = 0;
    acceptedIrqCount_ = 0;
    ptmIrqAssertCount_ = 0;
    ptmIrqAcknowledged_ = false;
    ptmCycleFraction_ = 0;
    waitNanosecondFraction_ = 0;

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

void Main6809Board::setTxIrqEnabled(bool enabled)
{
    usart_->setTxIrqEnabled(enabled);
    refreshSerialIrq();
}

void Main6809Board::queueSerialInput(BYTE value)
{
    usart_->receive(value);
    refreshSerialIrq();
}

bool Main6809Board::takeSerialOutput(BYTE& value)
{
    return usart_->takeOutput(value);
}

BYTE Main6809Board::serialStatus() const
{
    return usart_->status();
}

void Main6809Board::refreshSerialIrq()
{
    if(usart_->irqRequest()) mainIrqSources_|=0x80;
    else mainIrqSources_&=static_cast<BYTE>(~0x80);
    mainIrqStatus();
}

void Main6809Board::loadFloppy2d(const std::vector<BYTE>& raw)
{
    floppy_->load2d(raw);
    floppyWritePio_=false;
    floppyOffset_=256;
    floppyStatus_=0;
}

void Main6809Board::loadFloppyD88(const std::vector<BYTE>& d88)
{
    floppy_->loadD88(d88);
    floppyWritePio_=false;
    floppyOffset_=256;
    floppyStatus_=0;
}

std::array<BYTE,256> Main6809Board::floppyBootSector(unsigned sector) const
{
    if(!floppy_->has2d() || sector<1 || sector>16)
        throw std::invalid_argument("invalid IPL floppy sector");
    std::array<BYTE,256> bytes{};
    for(unsigned i=0;i<256;++i)bytes[i]=floppy_->read(0,(sector-1)*256+i);
    return bytes;
}

std::vector<BYTE> Main6809Board::floppyImage() const
{
    return floppy_->image2d();
}

bool Main6809Board::floppySectorOffset(std::size_t& offset)
{
    const unsigned track=floppy_->registerValue(0x19);
    const unsigned side=floppy_->registerValue(0x1c)&1;
    const unsigned sector=floppy_->registerValue(0x1a);
    const unsigned drive=floppy_->registerValue(0x1d)&3;
    if(!floppy_->sectorOffset(0,track,side,sector,drive,offset)) {
        floppyStatus_=0x10;
        floppyOffset_=256;
        ioRegisters_[0x50]|=0x40;
        return false;
    }
    return true;
}

void Main6809Board::writeFloppySector()
{
    std::size_t offset=0;
    floppyWritePio_=false;
    floppyStatus_=0;
    floppyOffset_=256;
    if(!floppySectorOffset(offset))return;
    const bool dma=(ioRegisters_[0x50]&0x04)!=0;
    if (!dma) {
        floppyWritePio_=true;
        floppyWriteOffset_=offset;
        floppyOffset_=0;
        floppy_->beginSector(0,floppy_->registerValue(0x19),floppy_->registerValue(0x1c)&1,
                             floppy_->registerValue(0x1a),floppy_->registerValue(0x1d)&3,true);
        floppyStatus_=floppy_->transferStatus();
        return;
    }
    const unsigned count=(ioRegisters_[0x42]<<8)|ioRegisters_[0x43];
    if((ioRegisters_[0x50]&0x01)==0 || count!=256) {
        floppyStatus_=0x10;
        ioRegisters_[0x50]|=0x40;
        return;
    }
    const bool down=(ioRegisters_[0x50]&0x08)!=0;
    const WORD startAddress=WORD((ioRegisters_[0x40]<<8)|ioRegisters_[0x41]);
    const std::uint32_t dmaBank=std::uint32_t(ioRegisters_[0x94]&0x0f)<<16;
    for(unsigned i=0;i<256;++i) {
        const WORD address=WORD(startAddress+(down?-int(i):int(i)));
        if((dmaBank|address)>=memory_.size()) {
            floppyStatus_=0x10;
            ioRegisters_[0x50]|=0x40;
            return;
        }
    }
    for(unsigned i=0;i<256;++i) {
        const WORD address=WORD(startAddress+(down?-int(i):int(i)));
        floppy_->write(0,offset+i,readPhysical(dmaBank|address));
    }
    // The completed 256-byte transfer advances the channel address and
    // exhausts the byte count, as a DMA engine would do.
    const WORD advanced=WORD(startAddress+(down?-256:256));
    ioRegisters_[0x40]=BYTE(advanced>>8);
    ioRegisters_[0x41]=BYTE(advanced);
    ioRegisters_[0x42]=0;
    ioRegisters_[0x43]=0;

    ioRegisters_[0x50]=static_cast<BYTE>((ioRegisters_[0x50]|0x80)&~0x40);
    // Completion is visible both in the per-channel status and in the
    // DMA common completion latch, until the channel status is read.
    refreshDmaCompletion();
}


void Main6809Board::readFloppySector()
{
    std::size_t offset=0;
    if(!floppySectorOffset(offset))return;
    for(unsigned i=0;i<256;++i)floppySector_[i]=floppy_->read(0,offset+i);
    floppyOffset_=0;
    floppyStatus_=0x02; // DRQ for programmed I/O
    const bool dma=(ioRegisters_[0x50]&0x04)!=0;
    if(!dma)floppy_->beginSector(0,floppy_->registerValue(0x19),floppy_->registerValue(0x1c)&1,
                                 floppy_->registerValue(0x1a),floppy_->registerValue(0x1d)&3,false);
    if(dma) {
        const unsigned count=(ioRegisters_[0x42]<<8)|ioRegisters_[0x43];
        if(count!=256) {
            floppyStatus_=0x10;
            ioRegisters_[0x50]|=0x40;
            return;
        }
        const bool down=(ioRegisters_[0x50]&0x08)!=0;
        const WORD startAddress=WORD((ioRegisters_[0x40]<<8)|ioRegisters_[0x41]);
        const std::uint32_t dmaBank=std::uint32_t(ioRegisters_[0x94]&0x0f)<<16;
        for(unsigned i=0;i<256;++i) {
            const WORD address=WORD(startAddress+(down?-int(i):int(i)));
            if((dmaBank|address)>=memory_.size()) {
                floppyStatus_=0x10;
                ioRegisters_[0x50]|=0x40;
                return;
            }
        }
        for(unsigned i=0;i<256;++i) {
            const WORD address=WORD(startAddress+(down?-int(i):int(i)));
            writePhysical(dmaBank|address,floppySector_[i]);
        }
        // The completed 256-byte transfer advances the channel address and
        // exhausts the byte count, as a DMA engine would do.
        const WORD advanced=WORD(startAddress+(down?-256:256));
        ioRegisters_[0x40]=BYTE(advanced>>8);
        ioRegisters_[0x41]=BYTE(advanced);
        ioRegisters_[0x42]=0;
        ioRegisters_[0x43]=0;
        floppyOffset_=256;
        floppyStatus_=0;
        ioRegisters_[0x50]=static_cast<BYTE>((ioRegisters_[0x50]|0x80)&~0x40);
        // Completion is visible both in the per-channel status and in the
        // DMA common completion latch, until the channel status is read.
        refreshDmaCompletion();
    }
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

CSim09::WaitState Main6809Board::cpuWaitState() const
{
    return core_.waitState();
}

void Main6809Board::advanceWaitTime(std::uint64_t nanoseconds)
{
    // Time passes while a real MC6809 is halted by SYNC or CWAI.
    // Advance the PTM's independent 9700 Hz clock from elapsed host time.
    constexpr std::uint64_t tickHz = 9700;
    const auto seconds = nanoseconds / 1000000000ULL;
    const auto remain = nanoseconds % 1000000000ULL;
    const auto numerator = remain * tickHz + waitNanosecondFraction_;
    const auto ticks = seconds * tickHz + numerator / 1000000000ULL;
    waitNanosecondFraction_ = numerator % 1000000000ULL;
    advancePtm(ticks);
}

std::uint64_t Main6809Board::nextPtmEventNanoseconds() const
{
    std::uint64_t soonest = UINT64_MAX;
    for (const auto& ch : ptm_)
        if (ch.running && ch.counter)
            soonest = std::min(soonest, std::uint64_t(ch.counter));
    if (soonest == UINT64_MAX) return soonest;
    return (soonest * 1000000000ULL + 9699) / 9700;
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

void Main6809Board::refreshDmaCompletion()
{
    bool anyComplete=false;
    bool irqComplete=false;
    for(unsigned channel=0;channel<4;++channel) {
        if((ioRegisters_[0x50+channel]&0x80)==0)continue;
        anyComplete=true;
        if((ioRegisters_[0x55]&(1u<<channel))!=0)irqComplete=true;
    }
    // Common DEND reports any completed channel, independent of IRQ masks.
    if(anyComplete)ioRegisters_[0x55]|=0x80;
    else ioRegisters_[0x55]&=static_cast<BYTE>(~0x80);
    if(irqComplete)mainIrqSources_|=0x10;
    else mainIrqSources_&=static_cast<BYTE>(~0x10);
    mainIrqStatus();
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
    if ((mainIrqSources_ & 0x10) != 0)
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

void Main6809Board::setCpuClockHz(std::uint64_t hz)
{
    if (hz == 0 || hz > 1000000000ULL)
        throw std::invalid_argument("CPU clock must be between 1 and 1e9 Hz");
    if (hz != cpuClockHz_) {
        cpuClockHz_ = hz;
        ptmCycleFraction_ = 0;
    }
}

void Main6809Board::advanceCpuCycles(std::uint64_t cycles)
{
    if (cpuClockHz_ == 0)
        throw std::logic_error("setCpuClockHz must be called before advanceCpuCycles");
    // Divide before multiplying to avoid overflow on large cycle batches.
    // The fractional numerator is preserved across calls, so splitting a
    // scheduler interval into smaller pieces does not change timer behavior.
    constexpr std::uint64_t kPtmInputHz = 19400 / 2;
    const std::uint64_t wholeSeconds = cycles / cpuClockHz_;
    const std::uint64_t remainderCycles = cycles % cpuClockHz_;
    if (wholeSeconds > UINT64_MAX / kPtmInputHz)
        throw std::overflow_error("PTM interval too large");
    const std::uint64_t extra = remainderCycles * kPtmInputHz + ptmCycleFraction_;
    const std::uint64_t fractionalTicks = extra / cpuClockHz_;
    const std::uint64_t wholeTicks = wholeSeconds * kPtmInputHz;
    if (wholeTicks > UINT64_MAX - fractionalTicks)
        throw std::overflow_error("PTM interval too large");
    ptmCycleFraction_ = extra % cpuClockHz_;
    advancePtm(wholeTicks + fractionalTicks);
}

void Main6809Board::refreshPtmIrq()
{
    bool active = false;
    for (unsigned i = 0; i < ptm_.size(); ++i)
        active |= (ptmStatus_ & (1u << i)) != 0 &&
                  (ptm_[i].control & 0x40) != 0;
    if (active)
        ptmStatus_ |= 0x80;
    else
        ptmStatus_ &= 0x7f;
    const bool asserted = active && !ptmIrqAcknowledged_;
    if (asserted && (mainIrqSources_ & 0x01) == 0) ++ptmIrqAssertCount_;
    setMainIrqSource(0x01, asserted);
    // The 6840 PTM is on the main board: its IRQ also reaches the
    // optional 8088 card through the shared system-bus IR0 line.
    if(mainBoard_)
        mainBoard_->setIrqLine(MainBoard::IrqLine::Timer,asserted);
}

void Main6809Board::advancePtm(std::uint64_t ticks)
{
    for (unsigned i = 0; i < ptm_.size(); ++i) {
        auto& ch = ptm_[i];
        if (!ch.running || ticks == 0)
            continue;
        if (ticks < ch.counter) {
            ch.counter -= static_cast<std::uint32_t>(ticks);
            continue;
        }
        const auto remaining = ticks - ch.counter;
        ptmStatus_ |= static_cast<BYTE>(1u << i);
        ptmIrqAcknowledged_ = false;
        const bool continuous = (ch.control & 0x28) == 0;
        if (continuous) {
            const std::uint64_t period = ch.latch == 0 ? 0x10000u : ch.latch;
            const auto mod = remaining % period;
            ch.counter = static_cast<std::uint32_t>(period - mod);
        } else {
            ch.counter = 0;
            ch.running = false;
        }
    }
    refreshPtmIrq();
}

BYTE Main6809Board::readPtm(BYTE port)
{
    if (port == 0x38)
        return 0;
    if (port == 0x39) {
        const BYTE status = ptmStatus_;
        // FD39 acknowledges IRQ, but leaves the per-channel status bits
        // available until the corresponding counter register is read.
        ptmIrqAcknowledged_ = true;
        refreshPtmIrq();
        return status;
    }
    const unsigned index = (port - 0x3a) / 2;
    auto& ch = ptm_[index];
    ptmStatus_ &= static_cast<BYTE>(~(1u << index));
    refreshPtmIrq();
    const WORD value = static_cast<WORD>(ch.counter);
    if ((port & 1) == 0)
        ioRegisters_[port + 1] = static_cast<BYTE>(value);
    return (port & 1) != 0 ? ioRegisters_[port] : static_cast<BYTE>(value >> 8);
}

void Main6809Board::writePtm(BYTE port, BYTE value)
{
    if (port == 0x38 || port == 0x39) {
        const unsigned index = port == 0x39 ? 1 :
            ((ptm_[1].control & 0x01) != 0 ? 0 : 2);
        ptm_[index].control = value;
        if (index == 0 && (value & 0x01) != 0) {
            ptm_.fill(PtmChannel{});
            ptm_[0].control = 0x01;
            ptmStatus_ = 0;
            ptmIrqAcknowledged_ = false;
        } else {
            ptmStatus_ &= static_cast<BYTE>(~(1u << index));
            const BYTE mode = value & 0x38;
            if (mode == 0x10 || mode == 0x30) {
                auto& ch = ptm_[index];
                ch.counter = ch.latch == 0 ? 0x10000u : ch.latch;
                ch.running = true;
            }
        }
        refreshPtmIrq();
        return;
    }
    ioRegisters_[port] = value;
    const unsigned index = (port - 0x3a) / 2;
    if ((port & 1) != 0) {
        auto& ch = ptm_[index];
        ch.latch = static_cast<WORD>((static_cast<WORD>(ioRegisters_[port - 1]) << 8) | value);
        ch.counter = ch.latch == 0 ? 0x10000u : ch.latch;
        ch.running = true;
    }
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
    case 0x38: case 0x39: case 0x3a: case 0x3b:
    case 0x3c: case 0x3d: case 0x3e: case 0x3f:
        return readPtm(reg);
    case 0x03:
        return mainIrqStatus();
    case 0x50: case 0x51: case 0x52: case 0x53: {
        // Original MainCpu.cpp DMA channel-control reads acknowledge DE.
        const BYTE status=ioRegisters_[reg];
        ioRegisters_[reg]=static_cast<BYTE>(status&~0x80);
        // A read acknowledges only this channel. Keep the common DMA
        // completion/IRQ indication while another channel is still done.
        refreshDmaCompletion();
        return status;
    }
    case 0x18: return floppyStatus_;
    case 0x19: case 0x1a: case 0x1c: case 0x1d: {
        BYTE busValue=0;
        if(mainBoard_ && mainBoard_->readIo(address,busValue))
            return busValue;
        return floppy_->registerValue(reg);
    }
    case 0x1b: {
        if(floppyOffset_==256)return 0;
        const BYTE value=floppy_->transferRead();
        ++floppyOffset_;
        if(floppyOffset_==256)floppyStatus_=0;
        return value;
    }
    case 0x06: {
        const BYTE value=usart_->readData();
        refreshSerialIrq();
        return value;
    }
    case 0x07:
        return serialStatus();
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
    case 0x38: case 0x39: case 0x3a: case 0x3b:
    case 0x3c: case 0x3d: case 0x3e: case 0x3f:
        writePtm(reg, value);
        break;
    case 0x02:
        ioOutputLatch_[0x02] = value;
        mainIrqStatus();
        break;
    case 0x2c:
        ioOutputLatch_[0x2c] = value;
        mainIrqStatus();
        break;
    case 0x19: case 0x1a: case 0x1c: case 0x1d:
        if(!mainBoard_ || !mainBoard_->writeIo(address,value))
            floppy_->writeRegister(reg,value);
        break;
    case 0x18:
        switch(FloppyDevice::decodeCommand(value)) {
        case FloppyDevice::Command::ReadSector: readFloppySector(); break;
        case FloppyDevice::Command::WriteSector: writeFloppySector(); break;
        default:
            floppy_->finishTransfer();
            floppyStatus_=0;
            floppyOffset_=256;
            floppyWritePio_=false;
            break;
        }
        break;
    case 0x1b:
        if(floppyWritePio_ && floppyOffset_<256) {
            floppy_->transferWrite(value);
            ++floppyOffset_;
            if(floppyOffset_==256) {
                floppyWritePio_=false;
                floppyStatus_=0;
            }
        }
        break;
    case 0x50: case 0x51: case 0x52: case 0x53:
        // DEND and BUSY are hardware status; reprogramming any channel
        // acknowledges only that channel's completed transfer.
        ioRegisters_[reg]=static_cast<BYTE>(value&~0xc0);
        refreshDmaCompletion();
        break;
    case 0x06:
        usart_->writeData(value);
        refreshSerialIrq();
        break;
    case 0x07:
        // The original USART accepts an instruction byte at this port.
        // For headless console I/O retain the Tx/Rx enable bits; baud/format
        // selection is handled by the frontend, not by guest CPU timing.
        usart_->writeCommand(value);
        refreshSerialIrq();
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
        if (reg == 0x55) {
            // HD6844 common IRQ/completion register: bit 7 is read-only.
            // IRQ enable writes must not acknowledge a pending completion.
            ioRegisters_[reg] = static_cast<BYTE>((ioRegisters_[reg] & 0x80) | (value & 0x7f));
            refreshDmaCompletion();
        } else {
            ioRegisters_[reg] = value;
        }
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
    // SYNC resumes on an asserted IRQ even if the CC.I mask prevents
    // vectoring. CWAI remains asleep until an unmasked interrupt arrives.
    if (waitState() == WaitState::Sync && board_.irqPending())
        clearSyncWait();
    // IRQ is level-sensitive. Check at instruction boundaries, and keep
    // the source pending while CC.I masks it. SimIRQ performs the complete
    // CPU state save and vectors through FFF8.
    constexpr BYTE kIrqMask = 0x10; // MC6809 CC.I
    if (board_.irqPending() && (regs.CC & kIrqMask) == 0) {
        SimIRQ();
        ++board_.acceptedIrqCount_;
        if (board_.cpuClockHz_ != 0)
            board_.advanceCpuCycles(
                variant_ == CpuType::HD6309 && (regs.MD & 0x01) != 0
                    ? 21 : 19); // W adds two native-mode stack cycles
        return;
    }
    SimONE();
    if (board_.cpuClockHz_ != 0 && LastInstructionCycles() != 0)
        board_.advanceCpuCycles(LastInstructionCycles());
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
    // ST_N blocks repeat NMIs in the existing CSim09 implementation.
    constexpr WORD kNmiInService = 0x0002;
    if ((regs.ST & kNmiInService) != 0)
        return;
    SimNMI();
    if (board_.cpuClockHz_ != 0)
        board_.advanceCpuCycles(
            variant_ == CpuType::HD6309 && (regs.MD & 0x01) != 0
                ? 21 : 19);
}

void Main6809Board::Core::firq()
{
    constexpr BYTE kFirqMask = 0x40; // CC.F
    if ((regs.CC & kFirqMask) != 0)
        return;
    SimFIRQ();
    if (board_.cpuClockHz_ != 0)
        board_.advanceCpuCycles(
            variant_ == CpuType::HD6309 && (regs.MD & 0x02) != 0
                ? ((regs.MD & 0x01) != 0 ? 21 : 19)
                : 10); // MD.F selects short or full register frame
}

CpuType Main6809Board::Core::type() const
{
    return variant_;
}
