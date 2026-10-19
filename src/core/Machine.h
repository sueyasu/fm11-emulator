#pragma once

#include "CpuBoard.h"
#include "MainBoard.h"
#include "RomFiles.h"

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
    DisplaySubsystem::VideoSnapshot captureDisplayVideo() const {return mainBoard_->displaySubsystem().captureVideo();}
    std::uint16_t displayCpuPc() const {return mainBoard_->displayCpu().pc();}
    std::uint64_t displayCpuClockHz() const {return displayCpuClockHz_;}
    std::uint64_t clock8088Hz() const {return clock8088Hz_;}
    std::uint64_t lastExecutionCycles() const {return lastExecutionCycles_;}
    // Advance the common main-board timebase regardless of active CPU.
    // Do not also advance the same interval through board.advanceCpuCycles().
    void advanceTimeNanoseconds(std::uint64_t nanoseconds);


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
    std::int64_t displayCycleCredit_ = 0;
};
