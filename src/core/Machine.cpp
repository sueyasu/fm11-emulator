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
    displayCycleCredit_=0;
    displayCycleFraction_=0;
    displayNanosecondFraction_=0;
    displayVsyncElapsedNs_=0;
    displayCpuInputHz_=0;
    synchronizeFd05Selection();
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
    // Reapplying the same configuration must not discard fractional
    // elapsed time or an instruction's outstanding cycle debt.
    if(displayCpuClockHz_==hz)return;
    displayCpuClockHz_=hz;
    displayCycleCredit_=0;
    displayCycleFraction_=0;
    displayNanosecondFraction_=0;
    displayCpuInputHz_=0;
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
        board->cpu().executeOne();
        lastExecutionCycles_=board->cpu().lastExecutionCycles();
        if(auto* cpu6809=dynamic_cast<Main6809Board*>(board)) {
            if(lastExecutionCycles_ && cpu6809->cpuClockHz()) {
                mainBoard_->advanceCpuCycles(lastExecutionCycles_,cpu6809->cpuClockHz());
                advanceDisplayCpuCycles(lastExecutionCycles_,cpu6809->cpuClockHz());
                cpu6809->syncPtmIrq();
            }
        } else if(dynamic_cast<Main8088Board*>(board) &&
                  lastExecutionCycles_ && clock8088Hz_) {
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
