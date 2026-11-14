#pragma once

#include "CpuBoard.h"
#include "MainBoard.h"
#include "RomFiles.h"
#include "DisplayBitmap.h"
#include "WorkerGroup.h"
#include "DeterministicDisplayScheduler.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <chrono>
#include <atomic>
#include <mutex>
#include <vector>

class Machine
{
public:
    ~Machine() {stopDisplayCpu();stopRealtimePtm();}
    // Opt-in real-time workers; unstarted machines keep deterministic scheduling.
    void startRealtimePtm();
    void stopRealtimePtm() noexcept;
    void pauseRealtimePtm();
    void resumeRealtimePtm();
    bool realtimePtmEnabled() const {return mainBoard_->realtimePtmEnabled();}
    void startDisplayCpu();
    void stopDisplayCpu() noexcept;
    void pauseDisplayCpu();
    void resumeDisplayCpu();
    bool displayCpuThreadEnabled() const {return displayThreadActive_.load(std::memory_order_acquire);}
    // Hold while reading live display memory or capturing/rendering a frame.
    std::unique_lock<std::recursive_mutex> lockDisplay() const {return mainBoard_->lockDisplay();}
    std::size_t addBoard(std::unique_ptr<CpuBoard> board);

    std::size_t boardCount() const;
    CpuBoard& board(std::size_t index);
    const CpuBoard& board(std::size_t index) const;

    void setActiveBoard(std::size_t index);
    // Optional FD05 bit-0 CPU-card selection. Disabled by default to
    // preserve explicit board selection during existing bring-up tests.
    void enableFd05Selection(bool enabled);
    bool fd05SelectionEnabled() const {return fd05SelectionEnabled_;}
    CpuBoard* activeBoard();
    const CpuBoard* activeBoard() const;

    void reset();
    void executeOne();
    void set8088ClockHz(std::uint64_t hz);
    void setZ80ClockHz(std::uint64_t hz);
    // Display CPU scheduling is opt-in until the hardware clock is resolved.
    void setDisplayCpuClockHz(std::uint64_t hz);
    // Worker-only clock multiplier. The sub-CPU is provisionally capped
    // at 10 MHz, including --speed 0 (unlimited main CPU).
    // Deterministic headless scheduling stays at displayCpuClockHz_.
    // Configure before starting the display worker.
    void setDisplayCpuSpeedMultiplier(unsigned multiplier);
    std::uint64_t displayCpuRealtimeClockHz() const {
        constexpr std::uint64_t kMaxDisplayCpuHz=10000000;
        if(displayCpuSpeedMultiplier_==0)return displayCpuClockHz_?kMaxDisplayCpuHz:0;
        return std::min(displayCpuClockHz_*displayCpuSpeedMultiplier_,kMaxDisplayCpuHz);
    }
    void loadDisplayRom(const DisplaySubsystem::RomImage& rom) {mainBoard_->displaySubsystem().loadRom(rom);}
    void loadDisplayRomFile(const std::string& path) {
        loadDisplayRom(RomFiles::load<DisplaySubsystem::kRomSize>(path));
    }
    void loadAnkRomFile(const std::string& path) {
        mainBoard_->displaySubsystem().loadAnkRom(
            RomFiles::load<DisplaySubsystem::kAnkRomSize>(path));
    }
    void loadKanjiRomFile(const std::string& path) {
        mainBoard_->displaySubsystem().loadKanjiRom(
            RomFiles::load<DisplaySubsystem::kKanjiRomSize>(path));
    }
    BYTE displayMemory(std::uint16_t address) const {return mainBoard_->displaySubsystem().readMemory(address);}
    // Explicit guest-side write stimulus for deterministic display tests.
    void writeDisplayMemory(std::uint16_t address,BYTE value) {
        mainBoard_->displaySubsystem().writeMemory(address,value);
    }
    const DisplaySubsystem& displaySubsystem() const {return mainBoard_->displaySubsystem();}
    void enqueueDisplayKeyCode(std::uint16_t code) {
        mainBoard_->displaySubsystem().enqueueKeyCode(code);
        if(displayWorker_)displayWorker_->notify();
    }
    DisplaySubsystem::VideoSnapshot captureDisplayVideo() const {return mainBoard_->displaySubsystem().captureVideo();}
    DisplayBitmap::Frame captureDisplayFrame(bool highResolution=false) const {
        return DisplayBitmap::captureFrame(mainBoard_->displaySubsystem(),highResolution);
    }
    // Diagnostic external stimulus; does not imply a mapped sub-CPU I/O port.
    void injectDisplayAttention() {mainBoard_->displaySubsystem().notifyMainAttention();}
    void injectBreakEvent() {mainBoard_->signalBreak();}
    BYTE displayBusyStatus() const {return mainBoard_->displayBusyStatus();}
    BYTE mainSubCpuControl() const {return mainBoard_->subCpuControl();}
    void setFd05WriteObserver(std::function<void(BYTE)> cb) {
        mainBoard_->setFd05WriteObserver(std::move(cb));
    }
    BYTE mainDisplayControl() const {return mainBoard_->displayControl();}
    BYTE mainDisplayPalette(unsigned index) const {return mainBoard_->displayPalette(index);}
    BYTE mainEventStatus() const {return mainBoard_->eventStatus();}
    std::uint64_t displaySharedWriteRevision() const {
        return mainBoard_->displaySubsystem().sharedWriteRevision();
    }
    bool displayHalted() const {
        return mainBoard_->displaySubsystem().haltRequested();
    }
    BYTE displaySharedByte(std::size_t offset) const {
        return mainBoard_->displaySubsystem().readShared(offset);
    }
    BYTE displayPhysicalVramByte(std::size_t offset) const {
        return mainBoard_->displaySubsystem().readPhysicalVram(offset);
    }
    std::uint16_t displayCpuPc() const {return mainBoard_->displayCpu().pc();}
    BYTE mainFd05Status() const {return mainBoard_->subCpuStatus();}
    BYTE subAfe0Status() const {return mainBoard_->displaySubsystem().readSubControl();}
    void setSubInstructionObserver(std::function<void(std::uint16_t)> fn) {
        mainBoard_->displayCpu().setInstructionObserver(std::move(fn));
    }
    void setSubKeyboardReadObserver(DisplaySubsystem::KeyboardReadObserver fn) {
        mainBoard_->displaySubsystem().setKeyboardReadObserver(std::move(fn));
    }
    void setSubAfe0ReadObserver(DisplaySubsystem::Afe0ReadObserver fn) {
        mainBoard_->displaySubsystem().setAfe0ReadObserver(std::move(fn));
    }
    void setDisplaySharedTraceObserver(DisplaySubsystem::SharedTraceObserver observer) {
        mainBoard_->displaySubsystem().setSharedTraceObserver(std::move(observer));
    }
    void setDisplayIoWriteObserver(DisplaySubsystem::IoWriteObserver observer) {
        mainBoard_->displaySubsystem().setIoWriteObserver(std::move(observer));
    }
    std::size_t displayNonzeroVramBytes() const {
        const auto& v=mainBoard_->displaySubsystem().physicalVram();
        return static_cast<std::size_t>(
            std::count_if(v.begin(),v.end(),[](BYTE b){return b!=0;}));
    }
    std::size_t displayPage() const {
        return mainBoard_->displaySubsystem().graphicsDisplayPage();
    }
    std::uint64_t displayRevision() const {
        return mainBoard_->displaySubsystem().videoRevision();
    }
    std::uint64_t displayCpuClockHz() const {return displayCpuClockHz_;}
    std::uint64_t clock8088Hz() const {return clock8088Hz_;}
    std::uint64_t clockZ80Hz() const {return clockZ80Hz_;}
    std::uint64_t lastExecutionCycles() const {return lastExecutionCycles_;}
    // Advance the common main-board timebase regardless of active CPU.
    // Do not also advance the same interval through board.advanceCpuCycles().
    void advanceTimeNanoseconds(std::uint64_t nanoseconds);


private:
    void synchronizeFd05Selection();
    bool fd05SelectionEnabled_=false;
    std::shared_ptr<MainBoard> mainBoard_=std::make_shared<MainBoard>();
    std::vector<std::unique_ptr<CpuBoard>> boards_;
    std::size_t active_board_ = 0;
    std::size_t primaryBoardIndex_ = 0, optionalBoardIndex_ = 0;
    bool hasPrimaryBoard_ = false, hasOptionalBoard_ = false;
    std::uint64_t lastExecutionCycles_ = 0;
    std::uint64_t clock8088Hz_ = 0;
    std::uint64_t clockZ80Hz_ = 4000000;
    std::uint64_t displayCpuClockHz_ = 0;
    unsigned displayCpuSpeedMultiplier_ = 1;
    DeterministicDisplayScheduler deterministicDisplay_{*mainBoard_};
    std::unique_ptr<WorkerGroup> ptmWorker_;
    bool ptmPaused_=false;
    std::unique_ptr<WorkerGroup> displayWorker_;
    std::atomic<bool> displayThreadActive_{false};
    std::atomic<std::uint64_t> displayResumeGeneration_{0};
    bool displayPaused_=false;
};
