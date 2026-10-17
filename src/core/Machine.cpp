#include "Machine.h"

#include <stdexcept>

std::size_t Machine::addBoard(std::unique_ptr<CpuBoard> board)
{
    if (!board)
        throw std::invalid_argument("board must not be null");

    boards_.push_back(std::move(board));
    return boards_.size() - 1;
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
    for (auto& board : boards_)
        board->reset();
}

void Machine::executeOne()
{
    if (auto* board = activeBoard())
        board->cpu().executeOne();
}
