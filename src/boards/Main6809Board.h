#pragma once

#include "CpuBoard.h"
#include "Sim09.h"
#include "FloppyDevice.h"
#include "MainBoard.h"

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
    void setMainBoard(std::shared_ptr<MainBoard> board) {
        mainBoard_=std::move(board);
        setFloppyDevice(mainBoard_->floppyDevice());
        usart_=mainBoard_->usartDevice();
        ptm_=mainBoard_->ptmDevice();
    }
    void setFloppyDevice(std::shared_ptr<FloppyDevice> device) { floppy_=std::move(device); }
    void loadFloppy2d(const std::vector<BYTE>& raw);
    void loadFloppyD88(const std::vector<BYTE>& d88);
    std::array<BYTE,256> floppyBootSector(unsigned sector) const;
    bool floppyDirty() const { return floppy_->dirty2d(); }
    std::vector<BYTE> floppyImage() const;
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

    // The first PTM timebase runs at PTMCLOCK/2 = 9700 Hz (16-bit mode,
    // internal clock select 0). CPU cycles are supplied by the scheduler.
    void setCpuClockHz(std::uint64_t hz);
    void advanceCpuCycles(std::uint64_t cycles);

    BYTE readCpu(WORD address);
    void writeCpu(WORD address, BYTE value);

    BYTE readPhysical(std::uint32_t address) const;
    void writePhysical(std::uint32_t address, BYTE value);

private:
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

    private:
        Main6809Board& board_;
        CpuType variant_;
    };

    std::vector<BYTE> memory_;
    BootRomImage bootRom_{};
    std::array<BYTE, kOnboardRamSize> onboardRam_{};

    // FDxx is the CPU-visible I/O register bank. The original implementation
    // also keeps separate FX/FY latches for output/control and device status.
    std::array<BYTE, 256> ioRegisters_{};
    std::array<BYTE, 256> ioOutputLatch_{};
    std::array<BYTE, 256> ioStatusLatch_{};
    std::shared_ptr<MainBoard> mainBoard_;
    std::shared_ptr<FloppyDevice> floppy_=std::make_shared<FloppyDevice>();
    bool floppyWritePio_ = false;
    std::size_t floppyWriteOffset_ = 0;
    std::array<BYTE,256> floppySector_{};
    unsigned floppyOffset_ = 256;
    BYTE floppyStatus_ = 0;
    std::shared_ptr<Usart8251> usart_=std::make_shared<Usart8251>();
    BYTE mainIrqSources_ = 0;
    BYTE extendedIrqSources_ = 0;
    BYTE mainIrqResult_ = 0;
    BYTE extendedIrqResult_ = 0;

    std::shared_ptr<Ptm6840> ptm_=std::make_shared<Ptm6840>();
    std::uint64_t acceptedIrqCount_ = 0;
    std::uint64_t ptmIrqAssertCount_ = 0;
    bool ptmIrqAcknowledged_ = false;
    std::uint64_t cpuClockHz_ = 0;
    std::uint64_t ptmCycleFraction_ = 0;
    std::uint64_t waitNanosecondFraction_ = 0;

    std::array<std::array<BYTE, 16>, 16> mmr_{};
    std::array<BYTE, 4> mmrControl_{};
    Core core_;
};
