#pragma once

#include "CpuBoard.h"
#include "MainBoard.h"

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
    CpuBoard* activeBoard();
    const CpuBoard* activeBoard() const;

    void reset();
    void executeOne();
    void set8088ClockHz(std::uint64_t hz);
    std::uint64_t clock8088Hz() const {return clock8088Hz_;}
    std::uint64_t lastExecutionCycles() const {return lastExecutionCycles_;}
    // Advance the common main-board timebase regardless of active CPU.
    // Do not also advance the same interval through board.advanceCpuCycles().
    void advanceTimeNanoseconds(std::uint64_t nanoseconds);


private:
    std::shared_ptr<MainBoard> mainBoard_=std::make_shared<MainBoard>();
    std::vector<std::unique_ptr<CpuBoard>> boards_;
    std::size_t active_board_ = 0;
    std::uint64_t lastExecutionCycles_ = 0;
    std::uint64_t clock8088Hz_ = 0;
};
