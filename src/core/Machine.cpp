#include "Machine.h"
#include "Main6809Board.h"
#include "Main8088Board.h"

#include <stdexcept>
#include <limits>

std::size_t Machine::addBoard(std::unique_ptr<CpuBoard> board)
{
    if (!board)
        throw std::invalid_argument("board must not be null");

    // The physical FDC/media belongs to the system bus, not either CPU card.
    if(auto* cpu6809=dynamic_cast<Main6809Board*>(board.get())) {
        cpu6809->setMainBoard(mainBoard_);
        cpu6809->setExternalClocking(true);
    }
    if(auto* cpu8088=dynamic_cast<Main8088Board*>(board.get()))
        cpu8088->bus().setMainBoard(mainBoard_);
    boards_.push_back(std::move(board));
    const auto index=boards_.size()-1;
    // A newly attached CPU card may complete the FD05 primary/optional
    // pair. Resolve the selection without relying on insertion order.
    synchronizeFd05Selection();
    return index;
}

std::size_t Machine::boardCount() const
{
    return boards_.size();
}

CpuBoard& Machine::board(std::size_t index)
{
    return *boards_.at(index);
}

const CpuBoard& Machine::board(std::size_t index) const
{
    return *boards_.at(index);
}

void Machine::setActiveBoard(std::size_t index)
{
    if (index >= boards_.size())
        throw std::out_of_range("active CPU board index out of range");

    active_board_ = index;
    // Explicit selection disables automatic FD05 arbitration. Callers can
    // re-enable it after completing manual setup/debugging.
    fd05SelectionEnabled_=false;
}

void Machine::enableFd05Selection(bool enabled)
{
    fd05SelectionEnabled_=enabled;
    if(enabled)synchronizeFd05Selection();
}

void Machine::synchronizeFd05Selection()
{
    if(!fd05SelectionEnabled_)return;
    std::size_t primary=boards_.size(),optional=boards_.size();
    for(std::size_t i=0;i<boards_.size();++i) {
        if(primary==boards_.size() &&
           dynamic_cast<Main6809Board*>(boards_[i].get()))primary=i;
        if(optional==boards_.size() &&
           dynamic_cast<Main8088Board*>(boards_[i].get()))optional=i;
    }
    // Do not invent a CPU card when only one kind is installed.
    if(primary==boards_.size() || optional==boards_.size())return;
    active_board_=mainBoard_->optionalCpuRequested()?optional:primary;
}

CpuBoard* Machine::activeBoard()
{
    return boards_.empty() ? nullptr : boards_[active_board_].get();
}

const CpuBoard* Machine::activeBoard() const
{
    return boards_.empty() ? nullptr : boards_[active_board_].get();
}

void Machine::reset()
{
    mainBoard_->reset();
    for (auto& board : boards_)
        board->reset();
    lastExecutionCycles_=0;
    measuredMainClockHz_=2000000;
    measuredMainAccesses_=0;
    mainTotalClockMs_=0;
    mainTotalAccesses_=0;
    serialTimerTicks_=0;
    referenceTimerPending_=false;
    referenceVsyncClockMs_=0;
    lastClockCalibration_=std::chrono::steady_clock::time_point{};
    displayVsyncElapsedNs_=0;
    synchronizeFd05Selection();
}

void Machine::signalReferenceTimer(std::uint64_t elapsedMs)
{
    // ChildView::OnTimer latches one event; main SimONE consumes it.
    // A slow host must not dispatch a burst of synthetic timer interrupts.
    signalDisplayVsync(elapsedMs);
    referenceTimerPending_=true;
}

void Machine::serviceReferenceMainTimer()
{
    if(!referenceTimerPending_)return;
    const bool vsync=true;
    if(vsync) {
        referenceTimerPending_=false;
        referenceVsyncClockMs_+=16;
    }
    const auto now=std::chrono::steady_clock::now();
    if(lastClockCalibration_==std::chrono::steady_clock::time_point{})
        lastClockCalibration_=now;
    const auto elapsed=std::chrono::duration_cast<std::chrono::milliseconds>(
        now-lastClockCalibration_).count();
    for(auto& board:boards_) {
        if(auto* cpu6809=dynamic_cast<Main6809Board*>(board.get())) {
            const auto accesses=cpu6809->memoryAccessCount();
            if(accesses>=measuredMainAccesses_ &&
               (referenceVsyncClockMs_>2000 || accesses-measuredMainAccesses_>1000000)) {
                const auto delta=accesses-measuredMainAccesses_;
                if(elapsed>0 && delta) {
                    mainTotalClockMs_+=static_cast<long double>(elapsed);
                    mainTotalAccesses_+=static_cast<long double>(delta);
                    // MainCpu::SimONE updates CpuClock only after the
                    // accumulated host interval exceeds one second.
                    if(mainTotalClockMs_>1000.0L)
                        measuredMainClockHz_=std::max<std::uint64_t>(
                            1,static_cast<std::uint64_t>(
                                mainTotalAccesses_*1000.0L/mainTotalClockMs_));
                    if(mainTotalClockMs_>180000.0L) {
                        mainTotalClockMs_/=10.0L;
                        mainTotalAccesses_/=10.0L;
                    }
                }
                measuredMainAccesses_=accesses;
                referenceVsyncClockMs_=0;
                lastClockCalibration_=now;
            }
        }
    }
    if(vsync && ++serialTimerTicks_>=4) {
        serialTimerTicks_=0;
        for(auto& board:boards_)
            if(auto* cpu6809=dynamic_cast<Main6809Board*>(board.get()))
                cpu6809->serviceSerialTimer();
    }
}

void Machine::advanceTimeNanoseconds(std::uint64_t nanoseconds)
{
    mainBoard_->advanceTimeNanoseconds(nanoseconds);
    advanceDisplayTime(nanoseconds);
    // The 6809 IRQ aggregator is still implemented in its CPU adapter.
    // Synchronize it with the main-board PTM after independent time advance.
    for(auto& board:boards_)
        if(auto* cpu6809=dynamic_cast<Main6809Board*>(board.get()))
            cpu6809->syncPtmIrq();
}

void Machine::set8088ClockHz(std::uint64_t hz)
{
    if(hz==0 || hz>1000000000ULL)
        throw std::invalid_argument("8088/V20 clock must be between 1 and 1e9 Hz");
    clock8088Hz_=hz;
}

void Machine::setDisplayCpuClockHz(std::uint64_t hz)
{
    if(hz>1000000000ULL)
        throw std::invalid_argument("display CPU clock must not exceed 1 GHz");
    if(displayCpuClockHz_==hz)return;
    displayCpuClockHz_=hz;
}

void Machine::advanceDisplayTime(std::uint64_t ns)
{
    // Only the legacy virtual timer remains for deterministic consumers.
    // Reference-mode SDL delivers the host's 16ms timer independently.
    if(!displayCpuClockHz_ || externalDisplayVsync_)return;
    constexpr std::uint64_t period=16000000;
    displayVsyncElapsedNs_+=ns;
    if(displayVsyncElapsedNs_>=period) {
        displayVsyncElapsedNs_%=period;
        mainBoard_->displayCpu().timerTick(16);
    }
}

void Machine::advanceDisplayCpuCycles(std::uint64_t cycles,std::uint64_t cpuHz)
{
    if(!displayCpuClockHz_ || !cycles || !cpuHz)return;
    constexpr std::uint64_t second=1000000000ULL;
    const auto whole=cycles/cpuHz;
    const auto rem=cycles%cpuHz;
    const auto ns=whole*second+(rem*second)/cpuHz;
    advanceDisplayTime(ns);
}

void Machine::executeOne()
{
    synchronizeFd05Selection();
    if (auto* board = activeBoard()) {
        // Sample the main-board USART before the 8088 tests its PIC.
        // No CPU-card bus conversion or interrupt controller is moved here.
        if (auto* cpu8088=dynamic_cast<Main8088Board*>(board))
            cpu8088->bus().refreshSystemIrq();
        const auto* refCpu=dynamic_cast<Main6809Board*>(board);
        const auto beforeAccesses=refCpu?refCpu->memoryAccessCount():0;
        if(referenceWallClock_) {
            serviceReferenceMainTimer();
            if(refCpu) {
                // MainCpu::SimONE: Fm11PTM(2) then Fm11IRQ() before IRQ
                // delivery. The accesses of this instruction are applied
                // only after the instruction has finished.
                mainBoard_->ptmDevice()->checkInstructionBoundary();
                const_cast<Main6809Board*>(refCpu)->syncPtmIrq();
            }
        }
        board->cpu().executeOne();
        lastExecutionCycles_=board->cpu().lastExecutionCycles();
        if(referenceWallClock_ && refCpu && measuredMainClockHz_) {
            const auto elapsedAccesses=refCpu->memoryAccessCount()-beforeAccesses;
            // Reference MainCpu::SysClock advances from memory accesses.
            // Feed the PTM in its own clock domain without nanosecond
            // truncation at every instruction.
            mainBoard_->advanceReferenceAccesses(
                elapsedAccesses,measuredMainClockHz_);
        }
        if(auto* cpu6809=dynamic_cast<Main6809Board*>(board)) {
            if(lastExecutionCycles_ && cpu6809->cpuClockHz()) {
                if(!referenceWallClock_)
                    mainBoard_->advanceCpuCycles(lastExecutionCycles_,cpu6809->cpuClockHz());
                advanceDisplayCpuCycles(lastExecutionCycles_,cpu6809->cpuClockHz());
                cpu6809->syncPtmIrq();
            }
        } else if(dynamic_cast<Main8088Board*>(board) &&
                  lastExecutionCycles_ && clock8088Hz_) {
            if(!referenceWallClock_)
                mainBoard_->advanceCpuCycles(lastExecutionCycles_,clock8088Hz_);
            advanceDisplayCpuCycles(lastExecutionCycles_,clock8088Hz_);
            for(auto& attached:boards_)
                if(auto* cpu6809=dynamic_cast<Main6809Board*>(attached.get()))
                    cpu6809->syncPtmIrq();
        }
        // Reference ChildView::SimProc: one main and one sub instruction
        // per scheduling turn, regardless of the main CPU cycle count.
        if(displayCpuClockHz_)mainBoard_->displayCpu().executeOne();
        // The instruction may have written FD05. Perform the next
        // instruction on the newly selected CPU card.
        synchronizeFd05Selection();
    }
}
