#include "Machine.h"
#include "Main6809Board.h"
#include "Main8088Board.h"
#include "MainZ80Board.h"

#include <stdexcept>
#include <limits>
#include <thread>
#include <chrono>

void Machine::startDisplayCpu()
{
    if(displayWorker_ || !displayCpuClockHz_)return;
    auto worker=std::make_unique<WorkerGroup>();
    using Clock=std::chrono::steady_clock;
    displayThreadActive_.store(true,std::memory_order_release);
    try {
        worker->start([this,last=Clock::now(),credit=std::int64_t{0},
                       fraction=std::uint64_t{0},
                       generation=displayResumeGeneration_.load()]() mutable {
            const auto currentGeneration=displayResumeGeneration_.load(std::memory_order_acquire);
            const auto now=Clock::now();
            if(generation!=currentGeneration) {
                generation=currentGeneration;
                credit=0;
                fraction=0;
                last=now;
            }
            // Accumulate 2 MHz display CPU cycles from real host nanoseconds.
            // Catch-up is capped at 20 ms to avoid replaying a host stall.
            constexpr std::uint64_t kSecond=1000000000ULL;
            const auto elapsed=std::chrono::duration_cast<std::chrono::nanoseconds>(now-last).count();
            last=now;
            const auto bounded=std::clamp<std::int64_t>(elapsed,0,20000000);
            const auto numerator=static_cast<std::uint64_t>(bounded)*displayCpuClockHz_+fraction;
            // Never accumulate unbounded debt when host CPU cannot keep up.
            credit=std::min<std::int64_t>(
                credit+static_cast<std::int64_t>(numerator/kSecond),
                static_cast<std::int64_t>(displayCpuClockHz_/50));
            fraction=numerator%kSecond;
            // Keep display-bus ownership bounded while avoiding one mutex
            // acquisition per emulated sub-CPU instruction.
            bool halted=false;
            for(unsigned i=0;i<8192 && credit>0 && !halted;) {
                auto guard=mainBoard_->lockDisplay();
                for(unsigned slice=0;slice<16 && i<8192 && credit>0;++slice,++i) {
                    if(!mainBoard_->displayCpu().executeOne()) {
                        credit=0;
                        halted=true;
                        break;
                    }
                    const auto cycles=mainBoard_->displayCpu().LastInstructionCycles();
                    credit-=static_cast<std::int64_t>(cycles?cycles:1);
                }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        });
        displayWorker_=std::move(worker);
        displayPaused_=false;
    }catch(...) {
        worker->stop();
        displayThreadActive_.store(false,std::memory_order_release);
        throw;
    }
}

void Machine::stopDisplayCpu() noexcept
{
    if(displayWorker_) {
        displayWorker_->stop();
        displayWorker_.reset();
    }
    displayThreadActive_.store(false,std::memory_order_release);
    displayPaused_=false;
}

void Machine::pauseDisplayCpu()
{
    if(!displayWorker_ || displayPaused_)return;
    displayWorker_->pause();
    displayPaused_=true;
}

void Machine::resumeDisplayCpu()
{
    if(!displayWorker_ || !displayPaused_)return;
    displayResumeGeneration_.fetch_add(1,std::memory_order_release);
    displayWorker_->resume();
    displayPaused_=false;
}

void Machine::startRealtimePtm()
{
    if(ptmWorker_)return;
    mainBoard_->startRealtimePtm();
    try {
        auto worker=std::make_unique<WorkerGroup>();
        worker->start([board=mainBoard_] {
            board->pollRealtimePtm();
            // Poll at most once per millisecond; PTM register reads/writes
            // also synchronize to host time under the same mutex.
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        });
        ptmWorker_=std::move(worker);
        ptmPaused_=false;
    } catch(...) {
        mainBoard_->stopRealtimePtm();
        throw;
    }
}

void Machine::stopRealtimePtm() noexcept
{
    if(ptmWorker_) {
        ptmWorker_->stop();
        ptmWorker_.reset();
    }
    mainBoard_->stopRealtimePtm();
    ptmPaused_=false;
}

void Machine::pauseRealtimePtm()
{
    if(!ptmWorker_ || ptmPaused_)return;
    ptmWorker_->pause();
    ptmPaused_=true;
}

void Machine::resumeRealtimePtm()
{
    if(!ptmWorker_ || !ptmPaused_)return;
    mainBoard_->rebaseRealtimePtm();
    ptmWorker_->resume();
    ptmPaused_=false;
}

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
    if(auto* cpuZ80=dynamic_cast<MainZ80Board*>(board.get())) {
        cpuZ80->setMainBoard(mainBoard_);
        for(const auto& attached:boards_)
            if(auto* cpu6809=dynamic_cast<Main6809Board*>(attached.get())) {
                cpuZ80->setPhysicalMemory(cpu6809->physicalMemory());
                break;
            }
    }
    if(auto* cpu6809=dynamic_cast<Main6809Board*>(board.get()))
        for(auto& attached:boards_)
            if(auto* cpuZ80=dynamic_cast<MainZ80Board*>(attached.get()))
                cpuZ80->setPhysicalMemory(cpu6809->physicalMemory());
    boards_.push_back(std::move(board));
    const auto index=boards_.size()-1;
    // Cache CPU-card positions once; FD05 selection is consulted on
    // instruction boundaries without scanning all attached cards.
    if(!hasPrimaryBoard_ && dynamic_cast<Main6809Board*>(boards_[index].get())) {
        primaryBoardIndex_=index;
        hasPrimaryBoard_=true;
    }
    if(!hasOptionalBoard_ &&
       (dynamic_cast<Main8088Board*>(boards_[index].get()) ||
        dynamic_cast<MainZ80Board*>(boards_[index].get()))) {
        optionalBoardIndex_=index;
        hasOptionalBoard_=true;
    }
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
    if(!fd05SelectionEnabled_ || !hasPrimaryBoard_ || !hasOptionalBoard_)return;
    active_board_=mainBoard_->optionalCpuRequested()
        ?optionalBoardIndex_:primaryBoardIndex_;
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
    const bool wasRealtime=bool(ptmWorker_);
    const bool hadDisplayWorker=bool(displayWorker_);
    if(hadDisplayWorker)stopDisplayCpu();
    if(wasRealtime)stopRealtimePtm();
    mainBoard_->reset();
    for (auto& board : boards_)
        board->reset();
    lastExecutionCycles_=0;
    deterministicDisplay_.reset();
    synchronizeFd05Selection();
    if(wasRealtime)startRealtimePtm();
    if(hadDisplayWorker)startDisplayCpu();
}

void Machine::advanceTimeNanoseconds(std::uint64_t nanoseconds)
{
    mainBoard_->advanceTimeNanoseconds(nanoseconds);
    if(!displayCpuThreadEnabled())deterministicDisplay_.advanceTime(nanoseconds);
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

void Machine::setZ80ClockHz(std::uint64_t hz)
{
    if(hz==0 || hz>1000000000ULL)
        throw std::invalid_argument("Z80 clock must be between 1 and 1e9 Hz");
    clockZ80Hz_=hz;
}

void Machine::setDisplayCpuClockHz(std::uint64_t hz)
{
    if(displayCpuThreadEnabled())
        throw std::logic_error("stop display CPU worker before changing its clock");
    if(hz>1000000000ULL)
        throw std::invalid_argument("display CPU clock must not exceed 1 GHz");
    displayCpuClockHz_=hz;
    deterministicDisplay_.setClockHz(hz);
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
                if(!realtimePtmEnabled())
                    mainBoard_->advanceCpuCycles(lastExecutionCycles_,cpu6809->cpuClockHz());
                if(!displayCpuThreadEnabled())
                    deterministicDisplay_.advanceCpuCycles(lastExecutionCycles_,cpu6809->cpuClockHz());
                cpu6809->syncPtmIrq();
            }
        } else if(dynamic_cast<MainZ80Board*>(board) &&
                  lastExecutionCycles_ && clockZ80Hz_) {
            if(!realtimePtmEnabled())
                mainBoard_->advanceCpuCycles(lastExecutionCycles_,clockZ80Hz_);
            if(!displayCpuThreadEnabled())
                    deterministicDisplay_.advanceCpuCycles(lastExecutionCycles_,clockZ80Hz_);
            for(auto& attached:boards_)
                if(auto* cpu6809=dynamic_cast<Main6809Board*>(attached.get()))
                    cpu6809->syncPtmIrq();
        } else if(dynamic_cast<Main8088Board*>(board) &&
                  lastExecutionCycles_ && clock8088Hz_) {
            if(!realtimePtmEnabled())
                mainBoard_->advanceCpuCycles(lastExecutionCycles_,clock8088Hz_);
            if(!displayCpuThreadEnabled())
                    deterministicDisplay_.advanceCpuCycles(lastExecutionCycles_,clock8088Hz_);
            for(auto& attached:boards_)
                if(auto* cpu6809=dynamic_cast<Main6809Board*>(attached.get()))
                    cpu6809->syncPtmIrq();
        }
        // The instruction may have written FD05. Perform the next
        // instruction on the newly selected CPU card.
        synchronizeFd05Selection();
    }
}
