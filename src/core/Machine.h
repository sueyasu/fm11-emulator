#pragma once

#include "CpuBoard.h"

#include <cstddef>
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

private:
    std::vector<std::unique_ptr<CpuBoard>> boards_;
    std::size_t active_board_ = 0;
};
