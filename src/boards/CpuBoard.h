#pragma once

#include "CpuCore.h"

#include <string_view>

class CpuBoard
{
public:
    virtual ~CpuBoard() = default;

    virtual std::string_view name() const = 0;
    virtual CpuCore& cpu() = 0;
    virtual const CpuCore& cpu() const = 0;
    virtual void reset() = 0;
};
