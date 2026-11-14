#include "Machine.h"
#include "Main6809Board.h"
#include "Main8088Board.h"
#include "MainZ80Board.h"

#include <stdexcept>
#include <limits>
#include <thread>
#include <chrono>

Machine::Machine()
{
    // Main-board FD05 writes come from any CPU card's I/O path.
    // The currently executing instruction retains its local CPU pointer;
    // the cached selection is used starting with the next instruction.
    mainBoard_->setCpuSelectionObserver([this](bool optional) {
        if(fd05SelectionEnabled_ && hasPrimaryBoard_ && hasOptionalBoard_)
            selectActiveBoard(optional?optionalBoardIndex_:primaryBoardIndex_);
    });
}

void Machine::setDisplayCpuSpeedMultiplier(unsigned multiplier)
{
    if(displayWorker_)
        throw std::logic_error("stop display CPU worker before changing multiplier");
    if(multiplier>100)
        throw std::invalid_argument("display CPU speed multiplier must be 0..100");
    displayCpuSpeedMultiplier_=multiplier;
}

void Machine::startDisplayCpu()
{
    if(displayWorker_ || !displayCpuClockHz_)return;
    const auto workerClockHz=displayCpuRealtimeClockHz();
    auto worker=std::make_unique<WorkerGroup>();
    using Clock=std::chrono::steady_clock;
    displayThreadActive_.store(true,std::memory_order_release);
    mainBoard_->setDisplayWakeObserver([wake=worker.get()] {wake->notify();});
    try {
        worker->start([this,wake=worker.get(),workerClockHz,last=Clock::now(),credit=std::int64_t{0},
                       fraction=std::uint64_t{0},
                       generation=displayResumeGeneration_.load()]() mutable {
            // Snapshot event generation before checking the guest state to
            // prevent a write arriving just before the wait being lost.
            const auto observed=wake->wakeGeneration();
            const auto currentGeneration=displayResumeGeneration_.load(std::memory_order_acquire);
            const auto now=Clock::now();
            if(generation!=currentGeneration) {
                generation=currentGeneration;
                credit=0;
                fraction=0;
                last=now;
            }
            // The display worker is always paced, including --speed 0.
            // Its 10 MHz limit is independent of the main CPU and PTM.
            constexpr std::uint64_t kSecond=1000000000ULL;
            const auto elapsed=std::chrono::duration_cast<std::chrono::nanoseconds>(now-last).count();
            const auto bounded=std::clamp<std::int64_t>(elapsed,0,20000000);
            const auto numerator=static_cast<std::uint64_t>(bounded)*workerClockHz+fraction;
            credit=std::min<std::int64_t>(
                credit+static_cast<std::int64_t>(numerator/kSecond),
                static_cast<std::int64_t>(workerClockHz/50));
            fraction=numerator%kSecond;
            last=now;
            // Keep display-bus ownership bounded while avoiding one mutex
            // acquisition per emulated sub-CPU instruction.
            bool halted=false;
            bool hardwareHalt=false;
            for(unsigned i=0;i<8192 && credit>0 && !halted;) {
                auto guard=mainBoard_->lockDisplay();
                for(unsigned slice=0;slice<16 && i<8192 && credit>0;++slice,++i) {
                    if(!mainBoard_->displayCpu().executeOne()) {
                        hardwareHalt=mainBoard_->displaySubsystem().haltRequested();
                        credit=0;
                        halted=true;
                        break;
                    }
                    const auto cycles=mainBoard_->displayCpu().LastInstructionCycles();
                    credit-=static_cast<std::int64_t>(cycles?cycles:1);
                }
            }
            if(halted) {
                // Hardware HALT wakes on FD05; a bounded fallback is kept
                // for external stimuli and emulated wait-state behavior.
                wake->waitFor(hardwareHalt?std::chrono::milliseconds(20)
                                          :std::chrono::microseconds(200),observed);
            } else if(credit<=0) {
                // Sleep only until the next instruction can be funded.
                // Avoid a tight spin on sub-microsecond cycle deficits.
                const auto missing=static_cast<std::uint64_t>(1-credit);
                constexpr std::uint64_t second=1000000000ULL;
                const auto ns=(missing*second+workerClockHz-1)/workerClockHz;
                wake->waitFor(std::chrono::nanoseconds(std::max<std::uint64_t>(ns,25000)),observed);
            } else {
                // Hit the bounded 8192-instruction batch while budget remains.
                std::this_thread::yield();
            }
        });
        displayWorker_=std::move(worker);
        displayPaused_=false;
    }catch(...) {
        mainBoard_->setDisplayWakeObserver({});
        worker->stop();
        displayThreadActive_.store(false,std::memory_order_release);
        throw;
    }
}

void Machine::stopDisplayCpu() noexcept
{
    if(displayWorker_) {
        mainBoard_->setDisplayWakeObserver({});
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
    CpuBoard* raw=boards_[index].get();
    ExecutionSlot slot;
    slot.board=raw;
    slot.core=&raw->cpu();
    slot.type=slot.core->type();
    if(auto* cpu6809=dynamic_cast<Main6809Board*>(raw)) {
        slot.kind=ExecutionKind::Main6809;
        slot.main6809=cpu6809;
        main6809Boards_.push_back(cpu6809);
        if(!hasPrimaryBoard_) {
            primaryBoardIndex_=index;
            hasPrimaryBoard_=true;
        }
    } else if(auto* cpu8088=dynamic_cast<Main8088Board*>(raw)) {
        slot.kind=ExecutionKind::Main8088;
        slot.main8088=cpu8088;
        if(!hasOptionalBoard_) {
            optionalBoardIndex_=index;
            hasOptionalBoard_=true;
        }
    } else if(dynamic_cast<MainZ80Board*>(raw)) {
        slot.kind=ExecutionKind::MainZ80;
        if(!hasOptionalBoard_) {
            optionalBoardIndex_=index;
            hasOptionalBoard_=true;
        }
    }
    executionSlots_.push_back(slot);
    // Adding a board can relocate executionSlots_; always refresh the
    // current value, even if automatic FD05 selection is disabled.
    if(fd05SelectionEnabled_ && hasPrimaryBoard_ && hasOptionalBoard_)
        synchronizeFd05Selection();
    else selectActiveBoard(active_board_);
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

    selectActiveBoard(index);
    // Explicit selection disables automatic FD05 arbitration. Callers can
    // re-enable it after completing manual setup/debugging.
    fd05SelectionEnabled_=false;
}

void Machine::configureFd05Boards(std::size_t mainIndex,std::size_t optionalIndex)
{
    if(mainIndex>=boards_.size() || optionalIndex>=boards_.size() ||
       mainIndex==optionalIndex)
        throw std::invalid_argument("invalid CPU selection wiring");
    primaryBoardIndex_=mainIndex;
    optionalBoardIndex_=optionalIndex;
    hasPrimaryBoard_=hasOptionalBoard_=true;
    if(fd05SelectionEnabled_)synchronizeFd05Selection();
}

void Machine::enableFd05Selection(bool enabled)
{
    fd05SelectionEnabled_=enabled;
    if(enabled)synchronizeFd05Selection();
}

void Machine::selectActiveBoard(std::size_t index)
{
    active_board_=index;
    activeExecution_=executionSlots_[index];
}

void Machine::synchronizeFd05Selection()
{
    if(!fd05SelectionEnabled_ || !hasPrimaryBoard_ || !hasOptionalBoard_)return;
    selectActiveBoard(mainBoard_->optionalCpuRequested()
        ?optionalBoardIndex_:primaryBoardIndex_);
}

CpuBoard* Machine::activeBoard()
{
    return activeExecution_.board;
}

const CpuBoard* Machine::activeBoard() const
{
    return activeExecution_.board;
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

void Machine::executeOne(bool ptmIrqSyncedBefore)
{
    // Selection and dispatch were cached at board attach/manual selection,
    // reset or an FD05 write. No per-instruction RTTI or FD05 polling.
    const auto execution=activeExecution_;
    if(!execution.core)return;

    // Only the optional 8088/V20 card needs a pre-instruction PIC refresh.
    if(execution.kind==ExecutionKind::Main8088)
        execution.main8088->bus().refreshSystemIrq();
    execution.core->executeOne();
    lastExecutionCycles_=execution.core->lastExecutionCycles();

    switch(execution.kind) {
    case ExecutionKind::Main6809:
        if(lastExecutionCycles_ && execution.main6809->cpuClockHz()) {
            const auto hz=execution.main6809->cpuClockHz();
            if(!realtimePtmEnabled())
                mainBoard_->advanceCpuCycles(lastExecutionCycles_,hz);
            if(!displayCpuThreadEnabled())
                deterministicDisplay_.advanceCpuCycles(lastExecutionCycles_,hz);
            // SDL samples the real-time PTM immediately before the
            // instruction for correct SYNC/CWAI and IRQ acceptance.
            // Avoid doing the same aggregation twice on that path.
            // Deterministic time advancement still needs post-step sync.
            if(!ptmIrqSyncedBefore || !realtimePtmEnabled())
                execution.main6809->syncPtmIrq();
        }
        break;
    case ExecutionKind::MainZ80:
    case ExecutionKind::Main8088: {
        const auto hz=execution.kind==ExecutionKind::MainZ80
            ?clockZ80Hz_:clock8088Hz_;
        if(lastExecutionCycles_ && hz) {
            if(!realtimePtmEnabled())
                mainBoard_->advanceCpuCycles(lastExecutionCycles_,hz);
            if(!displayCpuThreadEnabled())
                deterministicDisplay_.advanceCpuCycles(lastExecutionCycles_,hz);
            for(auto* cpu6809:main6809Boards_)
                cpu6809->syncPtmIrq();
        }
        break;
    }
    case ExecutionKind::Generic:
        break;
    }
    // FD05 writes synchronously changed activeExecution_ in MainBoard's
    // callback, but the instruction above completed on its original CPU.
}
