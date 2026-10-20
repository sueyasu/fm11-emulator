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
    // Reference CMainCpu::SimRESET: FD23 is the active-low boot-device
    // DIP switch (BootDrv), not a zero-initialized generic I/O latch.
    // Original emulator default: HDD off, 2HD/2D enabled, drive 0.
    ioRegisters_[0x23]=bootDip_;
    ioReadCounts_.fill(0);
    ioWriteCounts_.fill(0);
    lastIoWrites_.fill(0);
    floppyTypeIStatus_=false;
    floppyIndexPolls_=0;
    ioOutputLatch_.fill(0);
    ioStatusLatch_.fill(0);
    mainIrqSources_ = extendedIrqSources_ = 0;
    mainIrqResult_ = extendedIrqResult_ = 0;
    // Shared devices are reset by Machine/MainBoard, never by a CPU-card
    // reset. Standalone CPU cards still own and reset their own devices.
    if(!mainBoard_) {
        usart_->reset();
        ptm_->reset();
    }
    memoryAccessCount_=0;
    acceptedIrqCount_ = 0;
    ptmIrqAssertCount_ = 0;
    if(!mainBoard_)ptm_->irqAcknowledged = false;
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
    ++fdcReads_;
    std::size_t offset=0;
    if(!floppySectorOffset(offset)) {++fdcReadFailures_;return;}
    for(unsigned i=0;i<256;++i)floppySector_[i]=floppy_->read(0,offset+i);
    floppyOffset_=0;
    floppyStatus_=0x02; // DRQ for programmed I/O
    const bool dma=(ioRegisters_[0x50]&0x04)!=0;
    if(!dma)floppy_->beginSector(0,floppy_->registerValue(0x19),floppy_->registerValue(0x1c)&1,
                                 floppy_->registerValue(0x1a),floppy_->registerValue(0x1d)&3,false);
    if(dma) {
        const unsigned count=(ioRegisters_[0x42]<<8)|ioRegisters_[0x43];
        // Fm11DMA transfers up to the remaining DMA count. A count
        // shorter than a sector ends DMA successfully; a longer one
        // reports not-ended (B/R), not a WD179x record-not-found.
        const unsigned transfer=std::min(count,256u);
        const bool down=(ioRegisters_[0x50]&0x08)!=0;
        const WORD startAddress=WORD((ioRegisters_[0x40]<<8)|ioRegisters_[0x41]);
        const std::uint32_t dmaBank=std::uint32_t(ioRegisters_[0x94]&0x0f)<<16;
        for(unsigned i=0;i<transfer;++i) {
            const WORD address=WORD(startAddress+(down?-int(i):int(i)));
            if((dmaBank|address)>=memory_.size()) {
                ++fdcReadFailures_;
                floppyStatus_=0x10;
                ioRegisters_[0x50]|=0x40;
                return;
            }
        }
        for(unsigned i=0;i<transfer;++i) {
            const WORD address=WORD(startAddress+(down?-int(i):int(i)));
            writePhysical(dmaBank|address,floppySector_[i]);
        }
        // The completed 256-byte transfer advances the channel address and
        // exhausts the byte count, as a DMA engine would do.
        const WORD advanced=WORD(startAddress+(down?-int(transfer):int(transfer)));
        ioRegisters_[0x40]=BYTE(advanced>>8);
        ioRegisters_[0x41]=BYTE(advanced);
        const unsigned remaining=count-transfer;
        ioRegisters_[0x42]=BYTE(remaining>>8);
        ioRegisters_[0x43]=BYTE(remaining);
        floppyOffset_=256;
        floppyStatus_=0;
        if(remaining==0)ioRegisters_[0x50]=static_cast<BYTE>((ioRegisters_[0x50]|0x80)&~0x40);
        else ioRegisters_[0x50]|=0x40;
        // Completion is visible both in the per-channel status and in the
        // DMA common completion latch, until the channel status is read.
        refreshDmaCompletion();
    }
    ++fdcReadSuccesses_;
}

void Main6809Board::signalAttention()
{
    if(mainBoard_)mainBoard_->signalAttention();
    else ioRegisters_[0x04] |= 0x01;
}

void Main6809Board::signalBreak()
{
    if(mainBoard_)mainBoard_->signalBreak();
    else ioRegisters_[0x04] |= 0x02;
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
    return ptm_->nextEventNanoseconds();
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
    // Source Fm11IRQ: RS232C uses FD02 bits 1, 2 and 3 (0x0E).
    if ((mainIrqSources_ & 0x80) != 0 && (mask & 0x0e) != 0)
        value |= 0x80;
    if ((mainIrqSources_ & 0x40) != 0 && (mask & 0x80) != 0)
        value |= 0x40;
    // Source Fm11IRQ masks the DMA source by Fm11Ram[D55] bits 0-3.
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
    ptm_->refreshIrq();
    const bool asserted=ptm_->irqRequest();
    if(asserted && (mainIrqSources_&0x01)==0)++ptmIrqAssertCount_;
    setMainIrqSource(0x01,asserted);
    if(mainBoard_)mainBoard_->setIrqLine(MainBoard::IrqLine::Timer,asserted);
}

void Main6809Board::refreshKeyboardIrq()
{
    if(!mainBoard_ || (ioOutputLatch_[0x02]&0x80)==0 ||
       (mainIrqSources_&0x40)!=0)return;
    std::uint16_t code=0;
    if(!mainBoard_->displaySubsystem().takeMainKeyboardCode(code))return;
    ioRegisters_[0x00]=static_cast<BYTE>(code>>8);
    ioRegisters_[0x01]=static_cast<BYTE>(code);
    mainIrqSources_|=0x40;
    mainIrqStatus();
}

void Main6809Board::syncPtmIrq()
{
    refreshPtmIrq();
    refreshKeyboardIrq();
}

void Main6809Board::advancePtm(std::uint64_t ticks)
{
    ptm_->advance(ticks);
    refreshPtmIrq();
}

BYTE Main6809Board::readPtm(BYTE port)
{
    const BYTE value=ptm_->read(port);
    refreshPtmIrq();
    return value;
}

void Main6809Board::writePtm(BYTE port,BYTE value)
{
    ptm_->write(port,value);
    refreshPtmIrq();
}

BYTE Main6809Board::readCpu(WORD address)
{
    ++memoryAccessCount_;
    // 5000-5FFF is the 4 KiB boot ROM while FD93 bit 0 is clear.
    if (address >= 0x5000 && address <= 0x5fff && (mmrControl_[3] & 0x01) == 0)
        return bootRom_[address & 0x0fff];

    // 7C00-7FFF can be redirected by the window registers FD91/FD92.
    if (address >= 0x7c00 && address <= 0x7fff && (mmrControl_[3] & 0x40) != 0)
        return readPhysical(windowPhysicalAddress(address));

    // FC00-FFFF is fixed on-board space. FD00-FDFF is I/O; the remaining
    // three 256-byte regions are backed by the board's 4 KiB local RAM.
    if (address >= 0xfc00) {
        if (address>=0xfc80 && address<=0xfcff && mainBoard_)
            return mainBoard_->displaySubsystem().readShared(address-0xfc80);
        if ((address & 0xff00) == 0xfd00)
            return readIo(address);
        return onboardRam_[address & 0x0fff];
    }

    return readPhysical(translateCpuAddress(address));
}

void Main6809Board::writeCpu(WORD address, BYTE value)
{
    ++memoryAccessCount_;
    // Writes are ignored while the boot ROM is visible.
    if (address >= 0x5000 && address <= 0x5fff && (mmrControl_[3] & 0x01) == 0)
        return;

    if (address >= 0x7c00 && address <= 0x7fff && (mmrControl_[3] & 0x40) != 0) {
        writePhysical(windowPhysicalAddress(address), value);
        return;
    }

    if (address >= 0xfc00) {
        if (address>=0xfc80 && address<=0xfcff && mainBoard_)
            mainBoard_->displaySubsystem().writeShared(address-0xfc80,value);
        else if ((address & 0xff00) == 0xfd00)
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
    ++ioReadCounts_[reg];
    switch (reg) {
    case 0x38: case 0x39: case 0x3a: case 0x3b:
    case 0x3c: case 0x3d: case 0x3e: case 0x3f: {
        BYTE value=0;
        if(mainBoard_ && mainBoard_->readIo(address,value)) {
            syncPtmIrq();
            return value;
        }
        return readPtm(reg);
    }
    case 0x01: {
        // Original CMainCpu::GetIoByte(FD01) acknowledges the keyboard
        // interrupt and fetches the next queued key.
        const BYTE result=ioRegisters_[0x01];
        mainIrqSources_&=static_cast<BYTE>(~0x40);
        mainIrqStatus();
        refreshKeyboardIrq();
        return result;
    }
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
    case 0x33:
        return ioRegisters_[0x33]; // 2HD FDC PIO data latch
    case 0x30: {
        // Reference MainCpu::GetIoByte: return the status, acknowledge
        // INTRQ (bit 0), then merge the one-shot DRQ latch.
        const BYTE status=ioRegisters_[reg];
        ioRegisters_[reg]=static_cast<BYTE>(status&~0x01);
        return status;
    }
    case 0x36:
        // Reference 2HD drive-sense register: C0 | RDY0..RDY3.
        // The shared device currently supports drive zero only.
        return static_cast<BYTE>(0xc0 | (floppy_->has2hd() ? (1u<<floppy_->mountedDrive()) : 0));
    case 0x37: {
        // Reference MainCpu::GetIoByte: reading FDC IRQ status
        // acknowledges the extended FDC interrupt source.
        const BYTE result=ioRegisters_[0x37];
        setExtendedIrqSource(0x40,false);
        return result;
    }
    case 0x1f:
        // Reading the MiniFDC IRQ latch acknowledges its IRQ request.
        mainIrqSources_&=static_cast<BYTE>(~0x08);
        mainIrqStatus();
        return floppy_->registerValue(0x1f);
    case 0x18: {
        if(!floppyTypeIStatus_)return floppyStatus_;
        // The index hole produces a short pulse once per revolution.
        // Model its periodic assertion in polling-driven firmware even
        // when the guest has no dedicated index pulse clock yet.
        const bool index=(++floppyIndexPolls_%256u)<16u;
        return static_cast<BYTE>((floppyStatus_&~0x02u) |
            ((floppy_->has2d() && index)?0x02u:0));
    }
    case 0x19: case 0x1a: case 0x1c: case 0x1d: {
        BYTE busValue=0;
        if(mainBoard_ && mainBoard_->readIo(address,busValue))
            return busValue;
        return floppy_->registerValue(reg);
    }
    case 0x1b: {
        // Reference Fm11MiniFDCData returns the FD1B data latch when
        // DRQ is clear. ROM checks this latch before issuing READ SECTOR.
        if(floppyOffset_==256)return floppy_->registerValue(0x1b);
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
        BYTE value=0;
        if(mainBoard_ && mainBoard_->readIo(address,value))
            return value;
        // Standalone CPU-card fallback.
        value=ioRegisters_[reg];
        ioRegisters_[reg]&=static_cast<BYTE>(~0x03);
        return value;
    }
    case 0x05: {
        BYTE busValue=0;
        if(mainBoard_ && mainBoard_->readIo(address,busValue))
            return busValue;
        return ioRegisters_[reg];
    }
    default:
        if(reg>=0x20 && reg<=0x22 && mainBoard_) {
            BYTE value=0;
            if(mainBoard_->readIo(address,value))return value;
        }
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
    ++ioWriteCounts_[reg];
    lastIoWrites_[reg]=value;
    switch (reg) {
    case 0x38: case 0x39: case 0x3a: case 0x3b:
    case 0x3c: case 0x3d: case 0x3e: case 0x3f:
        if(!mainBoard_ || !mainBoard_->writeIo(address,value))
            writePtm(reg,value);
        else
            syncPtmIrq();
        break;
    case 0x02:
        ioOutputLatch_[0x02] = value;
        if(mainBoard_)
            mainBoard_->displaySubsystem().setKeyboardMainMode((value&0x80)!=0);
        if((value&0x80)==0)mainIrqSources_&=static_cast<BYTE>(~0x40);
        mainIrqStatus();
        refreshKeyboardIrq();
        break;
    case 0x2c:
        ioOutputLatch_[0x2c] = value;
        mainIrqStatus();
        break;
    case 0x34: case 0x35:
        // Reference MainCpu::SetIoByte: switching 2HD side/drive
        // clears both the controller status and its DRQ latch.
        ioRegisters_[0x30]=0;
        ioRegisters_[reg]=value;
        break;
    case 0x19: case 0x1a: case 0x1d:
        if(!mainBoard_ || !mainBoard_->writeIo(address,value))
            floppy_->writeRegister(reg,value);
        if(reg==0x1d)floppyStatus_=0;
        break;
    case 0x1c:
        // MainCpu.cpp keeps the side-selection latch separate from
        // the hardware IRQ-completion bit in the readable register.
        floppy_->writeRegister(0x1c,static_cast<BYTE>(
            (floppy_->registerValue(0x1c)&0xfe)|(value&1)));
        break;
    case 0x30: {
        // MainCpu::Fm11FDC: Type I RESTORE/SEEK/STEP need not access
        // the disk. Not Ready is returned only when Verify is requested.
        // The boot ROM issues RESTORE $08 with no 2HD disk present.
        BYTE status=0;
        const bool hasDisk=floppy_->present(ioRegisters_[0x35]&3);
        if((value&0x80)==0) {
            switch(value&0xe0) {
            case 0x00:
                if((value&0x10)==0)ioRegisters_[0x31]=0;
                else ioRegisters_[0x31]=ioRegisters_[0x33];
                break;
            case 0x20:
                if(value&0x10)
                    ioRegisters_[0x31]=static_cast<BYTE>(ioRegisters_[0x31]+1);
                break;
            case 0x40: ++ioRegisters_[0x31]; break;
            case 0x60: --ioRegisters_[0x31]; break;
            }
            if((value&0x04) && !hasDisk)status=0x80;
        } else if((value&0xc0)==0x80) {
            ++hdReadAttempts_;
            if(!hasDisk) {
                ++hdReadFailures_;
                ++hdFailuresByDrive_[ioRegisters_[0x35]&3];
                status=0x80;
            } else {
                // Only 2HD media can be accessed through this controller.
                // Unlike the MiniFDC, this controller uses DMA channel 1.
                const unsigned track=ioRegisters_[0x31];
                const unsigned sector=ioRegisters_[0x32];
                const unsigned side=ioRegisters_[0x34]&1;
                const unsigned drive=ioRegisters_[0x35]&3;
                ++hdAttemptsByDrive_[drive];
                hdLastTrack_=static_cast<BYTE>(track);
                hdLastSector_=static_cast<BYTE>(sector);
                hdLastSide_=static_cast<BYTE>(side);
                std::size_t offset=0;
                if(!floppy_->sectorOffset(1,track,side,sector,drive,offset)) {
                    ++hdReadFailures_;
                    ++hdFailuresByDrive_[drive];
                    ++hdBadChs_;
                    status=0x10; // Record Not Found
                } else {
                    const unsigned length=static_cast<unsigned>(floppy_->sectorLength(1));
                    const bool dma=(ioRegisters_[0x51]&0x04)!=0;
                    if(!dma) {
                        ++hdReadSuccesses_;
                        floppy_->beginSector(1,track,side,sector,drive,(value&0x20)!=0);
                        status=0x03;
                    } else {
                        const unsigned count=(unsigned(ioRegisters_[0x46])<<8)|ioRegisters_[0x47];
                        {
                            // The reference DMA transfers min(count,sector size).
                            // Count exhaustion is a successful DMA end, not RNF.
                            const unsigned transfer=std::min(count,length);
                            if(transfer)++hdReadSuccesses_;
                            else {
                                ++hdReadFailures_;
                                ++hdFailuresByDrive_[drive];
                                ++hdZeroDma_;
                            }
                            const bool down=(ioRegisters_[0x51]&0x08)!=0;
                            const WORD start=WORD((ioRegisters_[0x44]<<8)|ioRegisters_[0x45]);
                            const std::uint32_t bank=std::uint32_t(ioRegisters_[0x95]&15)<<16;
                            const bool write=(ioRegisters_[0x51]&0x01)!=0;
                            for(unsigned i=0;i<transfer;++i) {
                                const WORD address=WORD(start+(down?-int(i):int(i)));
                                const std::uint32_t physical=bank|address;
                                if(write)floppy_->write(1,offset+i,readPhysical(physical));
                                else writePhysical(physical,floppy_->read(1,offset+i));
                            }
                            const WORD next=WORD(start+(down?-int(transfer):int(transfer)));
                            ioRegisters_[0x44]=BYTE(next>>8);
                            ioRegisters_[0x45]=BYTE(next);
                            const unsigned rest=count-transfer;
                            ioRegisters_[0x46]=BYTE(rest>>8);
                            ioRegisters_[0x47]=BYTE(rest);
                            if(rest==0) {
                                ioRegisters_[0x51]=static_cast<BYTE>((ioRegisters_[0x51]|0x80)&~0x40);
                                refreshDmaCompletion();
                            } else {
                                ioRegisters_[0x51]|=0x40;
                            }
                        }
                    }
                }
            }
        } else if((value&0xf0)==0xd0) {
            status=0;
        } else if(!hasDisk) {
            status=0x80;
        }
        ioRegisters_[0x30]=status;
        ioRegisters_[0x37]=0x40;
        break;
    }
    case 0x33:
        if(floppy_->transferChannel()==1 && (floppy_->transferStatus()&0x02)!=0) {
            floppy_->transferWrite(value);
        }
        ioRegisters_[0x33]=value;
        break;
    case 0x18:
        // MainCpu::Fm11MiniFDC clears completion before processing a
        // command; when it finishes, FD1C and FD1F both carry IRQ bit 6.
        floppy_->writeRegister(0x1c,floppy_->registerValue(0x1c)&1);
        floppy_->writeRegister(0x1f,floppy_->registerValue(0x1c));
        floppyTypeIStatus_=(value&0x80)==0;
        switch(FloppyDevice::decodeCommand(value)) {
        case FloppyDevice::Command::ReadSector: readFloppySector(); break;
        case FloppyDevice::Command::WriteSector: writeFloppySector(); break;
        default:
            floppy_->finishTransfer();
            floppyOffset_=256;
            floppyWritePio_=false;
            // WD179x Type-I commands update the track register. The boot
            // ROM uses RESTORE before attempting to read IPL sectors.
            // Previously these commands were all silently ignored.
            switch(value&0xf0) {
            case 0x00: // RESTORE
                floppy_->writeRegister(0x19,0);
                break;
            case 0x10: // SEEK (target supplied in data register)
                floppy_->writeRegister(0x19,floppy_->registerValue(0x1b));
                break;
            case 0x20: case 0x30: // STEP
            case 0x40: case 0x50: // STEP IN
            case 0x60: case 0x70: // STEP OUT
                // STEP without a remembered direction remains at current
                // track; STEP IN/OUT update the track counter.
                if((value&0xe0)==0x40 && floppy_->registerValue(0x19)<255)
                    floppy_->writeRegister(0x19,floppy_->registerValue(0x19)+1);
                if((value&0xe0)==0x60 && floppy_->registerValue(0x19)>0)
                    floppy_->writeRegister(0x19,floppy_->registerValue(0x19)-1);
                break;
            default:
                break;
            }
            // WD179x Type-I status: bit 2 is asserted at track zero.
            // The boot ROM checks this after RESTORE to verify that
            // the selected drive has reached its home position.
            floppyStatus_=((value&0xf0)==0xd0) ? 0 : static_cast<BYTE>(
                (floppy_->has2d()?0:0x80) |
                (floppy_->registerValue(0x19)==0?0x04:0));
            // Original Fm11MiniFDC unconditionally latches completion
            // for Type I and force-interrupt commands.
            floppy_->writeRegister(0x1c,floppy_->registerValue(0x1c)|0x40);
            floppy_->writeRegister(0x1f,floppy_->registerValue(0x1c));
            break;
        }
        // The reference Fm11MiniFDC always raises command completion,
        // including Type-II DMA reads and errors. FD1F polls wait for
        // this bit and cannot advance if it is left at zero.
        floppy_->writeRegister(0x1c,static_cast<BYTE>(floppy_->registerValue(0x1c)|0x40));
        floppy_->writeRegister(0x1f,floppy_->registerValue(0x1c));
        break;
    case 0x1b:
        // The WD179x data register is also the SEEK target and must retain
        // writes even when no sector transfer is active.
        floppy_->writeRegister(0x1b,value);
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
        if(!mainBoard_ || !mainBoard_->writeIo(address,value))
            ioRegisters_[reg]&=static_cast<BYTE>(~0x01);
        // Preserve the card-side output latch used by existing callers.
        ioOutputLatch_[reg]=0x80;
        break;
    case 0xa0:
    case 0x98: case 0x99: case 0x9a: case 0x9b:
    case 0x9c: case 0x9d: case 0x9e: case 0x9f:
        ioRegisters_[reg]=reg==0xa0?value:static_cast<BYTE>(value&0x0f);
        if(mainBoard_)mainBoard_->writeIo(address,value);
        break;
    case 0x05:
        // The control latch is a main-board device. Keep the standalone
        // card's previous behavior when no main board is attached.
        ioOutputLatch_[reg]=value;
        if(!mainBoard_ || !mainBoard_->writeIo(address,value)) {
            if(value&0x80) {
                ioRegisters_[reg]|=0x80;
                ioStatusLatch_[reg]&=0x7f;
            }
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
        if (reg>=0x20 && reg<=0x22 && mainBoard_)
            mainBoard_->writeIo(address,value);
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
    lastCycles_=0;
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
        lastCycles_=variant_ == CpuType::HD6309 && (regs.MD & 0x01) != 0
                    ? 21 : 19; // W adds two native-mode stack cycles
        if(board_.cpuClockHz_ != 0 && !board_.externalClocking_)
            board_.advanceCpuCycles(lastCycles_);
        return;
    }
    SimONE();
    lastCycles_=LastInstructionCycles();
    if(board_.cpuClockHz_ != 0 && !board_.externalClocking_ && lastCycles_ != 0)
        board_.advanceCpuCycles(lastCycles_);
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
    if (board_.cpuClockHz_ != 0 && !board_.externalClocking_)
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
    if (board_.cpuClockHz_ != 0 && !board_.externalClocking_)
        board_.advanceCpuCycles(
            variant_ == CpuType::HD6309 && (regs.MD & 0x02) != 0
                ? ((regs.MD & 0x01) != 0 ? 21 : 19)
                : 10); // MD.F selects short or full register frame
}

CpuType Main6809Board::Core::type() const
{
    return variant_;
}
