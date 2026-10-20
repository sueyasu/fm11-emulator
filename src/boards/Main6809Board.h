#pragma once

#include "CpuBoard.h"
#include "Sim09.h"
#include "FloppyDevice.h"
#include "MainBoard.h"
#include "RomFiles.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <vector>
#include <memory>

class Main6809Board final : public CpuBoard
{
public:
    static constexpr std::size_t kBootRomSize = 0x1000;
    using BootRomImage = std::array<BYTE, kBootRomSize>;

    explicit Main6809Board(CpuType variant = CpuType::HD6309);

    std::string_view name() const override;
    CpuCore& cpu() override;
    const CpuCore& cpu() const override;
    void reset() override;

    void loadBootRom(const BootRomImage& image);
    void loadBootRomFile(const std::string& path) {loadBootRom(RomFiles::load<kBootRomSize>(path));}
    void setMainBoard(std::shared_ptr<MainBoard> board) {
        mainBoard_=std::move(board);
        setFloppyDevice(mainBoard_->floppyDevice());
        usart_=mainBoard_->usartDevice();
        ptm_=mainBoard_->ptmDevice();
        // Reference SubCpu::SetIoByte(AFF0-AFF2) writes Fm11Ram[D20-D22].
        mainBoard_->displaySubsystem().setSubDmaRegisterWriter(
            [this](unsigned offset,BYTE value) {
                writeCpu(static_cast<WORD>(0xfd20+offset),value);
            });
    }
    void setFloppyDevice(std::shared_ptr<FloppyDevice> device) { floppy_=std::move(device); }
    void loadFloppy2d(const std::vector<BYTE>& raw);
    void loadFloppyD88(const std::vector<BYTE>& d88);
    void setFloppyDrive(unsigned drive) {floppy_->setMountedDrive(drive);}
    void setBootDip(BYTE value) {bootDip_=value;}
    BYTE bootDip() const {return bootDip_;}
    std::array<BYTE,256> floppyBootSector(unsigned sector) const;
    bool floppyDirty() const { return floppy_->dirty2d(); }
    std::vector<BYTE> floppyImage() const;
    std::uint64_t fdcReadCommands() const {return fdcReads_;}
    std::uint64_t hdReadAttempts() const {return hdReadAttempts_;}
    std::uint64_t hdReadSuccesses() const {return hdReadSuccesses_;}
    std::uint64_t hdReadFailures() const {return hdReadFailures_;}
    std::uint64_t hdReadAttemptsForDrive(unsigned d) const {return hdAttemptsByDrive_.at(d);}
    std::uint64_t hdReadFailuresForDrive(unsigned d) const {return hdFailuresByDrive_.at(d);}
    std::uint64_t hdBadChs() const {return hdBadChs_;}
    std::uint64_t hdZeroDma() const {return hdZeroDma_;}
    BYTE hdLastTrack() const {return hdLastTrack_;}
    BYTE hdLastSector() const {return hdLastSector_;}
    BYTE hdLastSide() const {return hdLastSide_;}
    std::uint64_t fdcSuccessfulReads() const {return fdcReadSuccesses_;}
    std::uint64_t fdcFailedReads() const {return fdcReadFailures_;}
    BYTE fdcStatus() const {return floppyStatus_;}
    std::uint64_t ioReadCount(unsigned reg) const {return ioReadCounts_.at(reg);}
    std::uint64_t ioWriteCount(unsigned reg) const {return ioWriteCounts_.at(reg);}
    BYTE lastIoWrite(unsigned reg) const {return lastIoWrites_.at(reg);}

    BYTE fdcTrack() const {return floppy_->registerValue(0x19);}
    BYTE fdcSector() const {return floppy_->registerValue(0x1a);}
    BYTE fdcDrive() const {return floppy_->registerValue(0x1d)&3;}
    void clearFloppyDirty() { floppy_->clearDirty2d(); }

    // Board-local event inputs used by the main-system control registers.
    // Device boards can use these without reaching into the CPU core.
    void signalAttention();
    void signalBreak();

    // Headless main RS-232C port (FD06 data, FD07 command/status).
    // The frontend owns terminal transport and may feed/drain bytes here.
    void setTxIrqEnabled(bool enabled);
    void queueSerialInput(BYTE value);
    bool takeSerialOutput(BYTE& value);

    // Device IRQ sources use the bit positions of FD03 and FD2C.
    void setMainIrqSource(BYTE sourceBit, bool asserted);
    void setExtendedIrqSource(BYTE sourceBit, bool asserted);
    bool irqPending() const;
    std::uint64_t acceptedIrqCount() const { return acceptedIrqCount_; }
    std::uint64_t ptmIrqAssertCount() const { return ptmIrqAssertCount_; }
    CSim09::WaitState cpuWaitState() const;
    void advanceWaitTime(std::uint64_t nanoseconds);
    std::uint64_t nextPtmEventNanoseconds() const;

    // Advance the PTM by input clock pulses. The scheduler will later
    // convert emulated CPU time to these PTM ticks.
    void advancePtm(std::uint64_t ticks);
    void syncPtmIrq(); // shared main-board clock changed PTM independently
    // Refresh periodic USART interrupt request from the machine scheduler.
    void serviceSerialTimer() {refreshSerialIrq();}

    // The first PTM timebase runs at PTMCLOCK/2 = 9700 Hz (16-bit mode,
    // internal clock select 0). CPU cycles are supplied by the scheduler.
    void setCpuClockHz(std::uint64_t hz);
    std::uint64_t cpuClockHz() const {return cpuClockHz_;}
    std::uint64_t memoryAccessCount() const {return memoryAccessCount_;}
    void setExternalClocking(bool enabled) {externalClocking_=enabled;}
    void advanceCpuCycles(std::uint64_t cycles);

    BYTE readCpu(WORD address);
    void writeCpu(WORD address, BYTE value);

    BYTE readPhysical(std::uint32_t address) const;
    void writePhysical(std::uint32_t address, BYTE value);

private:
    std::uint64_t memoryAccessCount_=0;
    BYTE bootDip_=0x9f;
    static constexpr std::size_t kPhysicalMemorySize = 1u << 20;
    static constexpr std::size_t kOnboardRamSize = 0x1000;
    static constexpr WORD kMmrPageSize = 0x1000;

    bool isMmrRegister(WORD address) const;
    BYTE readMmrRegister(WORD address) const;
    void writeMmrRegister(WORD address, BYTE value);
    BYTE readIo(WORD address);
    BYTE readPtm(BYTE port);
    void writePtm(BYTE port, BYTE value);
    void refreshPtmIrq();
    void refreshKeyboardIrq();
    BYTE mainIrqStatus();
    void refreshDmaCompletion();
    BYTE extendedIrqStatus();
    void readFloppySector();
    void writeFloppySector();
    bool floppySectorOffset(std::size_t& offset);
    BYTE serialStatus() const;
    void refreshSerialIrq();
    void writeIo(WORD address, BYTE value);
    std::uint32_t translateCpuAddress(WORD address) const;
    std::uint32_t windowPhysicalAddress(WORD address) const;

    class Core final : public CSim09, public CpuCore
    {
    public:
        Core(Main6809Board& board, CpuType variant);

        BYTE GetMemByte(WORD address) override;
        void SetMemByte(WORD address, BYTE value) override;

        void reset() override;
        void executeOne() override;
        std::uint32_t pc() const override;
        void irq() override;
        void nmi() override;
        void firq() override;
        CpuType type() const override;
        std::uint64_t lastExecutionCycles() const override {return lastCycles_;}

    private:
        Main6809Board& board_;
        CpuType variant_;
        std::uint64_t lastCycles_=0;
    };

    std::vector<BYTE> memory_;
    BootRomImage bootRom_{};
    std::array<BYTE, kOnboardRamSize> onboardRam_{};

    // FDxx is the CPU-visible I/O register bank. The original implementation
    // also keeps separate FX/FY latches for output/control and device status.
    std::array<BYTE, 256> ioRegisters_{};
    std::array<std::uint64_t,256> ioReadCounts_{};
    std::array<std::uint64_t,256> ioWriteCounts_{};
    std::array<BYTE,256> lastIoWrites_{};
    std::array<BYTE, 256> ioOutputLatch_{};
    std::array<BYTE, 256> ioStatusLatch_{};
    std::shared_ptr<MainBoard> mainBoard_;
    std::shared_ptr<FloppyDevice> floppy_=std::make_shared<FloppyDevice>();
    bool floppyWritePio_ = false;
    std::size_t floppyWriteOffset_ = 0;
    std::array<BYTE,256> floppySector_{};
    unsigned floppyOffset_ = 256;
    BYTE floppyStatus_ = 0;
    bool floppyTypeIStatus_ = false;
    std::uint32_t floppyIndexPolls_ = 0;
    std::uint64_t fdcReads_=0,fdcReadSuccesses_=0,fdcReadFailures_=0;
    std::uint64_t hdReadAttempts_=0,hdReadSuccesses_=0,hdReadFailures_=0;
    std::array<std::uint64_t,4> hdAttemptsByDrive_{},hdFailuresByDrive_{};
    std::uint64_t hdBadChs_=0,hdZeroDma_=0;
    BYTE hdLastTrack_=0,hdLastSector_=0,hdLastSide_=0;
    std::shared_ptr<Usart8251> usart_=std::make_shared<Usart8251>();
    BYTE mainIrqSources_ = 0;
    BYTE extendedIrqSources_ = 0;
    BYTE mainIrqResult_ = 0;
    BYTE extendedIrqResult_ = 0;

    std::shared_ptr<Ptm6840> ptm_=std::make_shared<Ptm6840>();
    std::uint64_t acceptedIrqCount_ = 0;
    std::uint64_t ptmIrqAssertCount_ = 0;
    bool ptmIrqAcknowledged_ = false;
    bool externalClocking_=false;
    std::uint64_t cpuClockHz_ = 0;
    std::uint64_t ptmCycleFraction_ = 0;
    std::uint64_t waitNanosecondFraction_ = 0;

    std::array<std::array<BYTE, 16>, 16> mmr_{};
    std::array<BYTE, 4> mmrControl_{};
    Core core_;
};
