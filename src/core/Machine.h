#pragma once

#include "CpuBoard.h"
#include "MainBoard.h"
#include "RomFiles.h"
#include "DisplayBitmap.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

class Machine
{
public:
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
    // Display CPU scheduling is opt-in until the hardware clock is resolved.
    void setDisplayCpuClockHz(std::uint64_t hz);
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
    const DisplaySubsystem& displaySubsystem() const {return mainBoard_->displaySubsystem();}
    void enqueueDisplayKeyCode(std::uint16_t code) {mainBoard_->displaySubsystem().enqueueKeyCode(code);}
    void setDisplayKeyboardEnabled(bool enabled) {mainBoard_->displaySubsystem().setKeyboardEnabled(enabled);}
    void disableDisplayKeyboardAfterStartupKey(bool enabled) {mainBoard_->displaySubsystem().disableKeyboardAfterStartupKey(enabled);}
    DisplaySubsystem::VideoSnapshot captureDisplayVideo() const {return mainBoard_->displaySubsystem().captureVideo();}
    DisplayBitmap::Frame captureDisplayFrame(bool highResolution=false) const {
        return DisplayBitmap::captureFrame(mainBoard_->displaySubsystem(),highResolution);
    }
    // Diagnostic external stimulus; does not imply a mapped sub-CPU I/O port.
    void injectDisplayAttention() {mainBoard_->displaySubsystem().notifyMainAttention();}
    void injectBreakEvent() {mainBoard_->signalBreak();}
    BYTE displayBusyStatus() const {return mainBoard_->displayBusyStatus();}
    BYTE mainSubCpuControl() const {return mainBoard_->subCpuControl();}
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
    std::uint64_t lastExecutionCycles() const {return lastExecutionCycles_;}
    // Advance the common main-board timebase regardless of active CPU.
    // Do not also advance the same interval through board.advanceCpuCycles().
    void advanceTimeNanoseconds(std::uint64_t nanoseconds);
    // SDL may drive the 16 ms display VSync from host monotonic time.
    // Default stays emulated time for headless and deterministic tests.
    void setExternalDisplayVsync(bool enabled) {externalDisplayVsync_=enabled; displayVsyncElapsedNs_=0;}
    void signalDisplayVsync(std::uint64_t elapsedMs=16) {
        if(displayCpuClockHz_)mainBoard_->displayCpu().timerTick(elapsedMs);
    }


private:
    void synchronizeFd05Selection();
    void advanceDisplayTime(std::uint64_t ns);
    void advanceDisplayCpuCycles(std::uint64_t cycles,std::uint64_t cpuHz);
    bool fd05SelectionEnabled_=false;
    std::shared_ptr<MainBoard> mainBoard_=std::make_shared<MainBoard>();
    std::vector<std::unique_ptr<CpuBoard>> boards_;
    std::size_t active_board_ = 0;
    std::uint64_t lastExecutionCycles_ = 0;
    std::uint64_t clock8088Hz_ = 0;
    std::uint64_t displayCpuClockHz_ = 0;
    std::uint64_t displayCycleFraction_ = 0;
    std::uint64_t displayCpuInputHz_ = 0;
    std::uint64_t displayNanosecondFraction_ = 0;
    std::uint64_t displayVsyncElapsedNs_ = 0;
    bool externalDisplayVsync_ = false;
    std::int64_t displayCycleCredit_ = 0;
};
