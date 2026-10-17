#pragma once

#include <cstdint>

enum class CpuType {
    MC6809,
    HD6309,
    I8088,
    NECV20,
    Z80,
};

class CpuCore
{
public:
    virtual ~CpuCore() = default;

    virtual void reset() = 0;
    virtual void executeOne() = 0;
    virtual std::uint32_t pc() const = 0;
    virtual void irq() = 0;
    virtual void nmi() = 0;
    virtual void firq() {}
    virtual CpuType type() const = 0;
};
