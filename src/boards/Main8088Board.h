#pragma once
#include "CpuBoard.h"
#include "Main8088Bus.h"
#include <array>
#include <cstdint>
#include <stdexcept>

// First-stage execution adapter for the original Sim86-style register layout.
// Only ROM bring-up instructions are implemented here; unimplemented opcodes
// raise an explicit exception rather than silently behaving like NOP.
class Main8088Board final : public CpuBoard {
public:
    explicit Main8088Board(CpuType variant=CpuType::I8088);
    std::string_view name() const override;
    CpuCore& cpu() override;
    const CpuCore& cpu() const override;
    void reset() override;
    Main8088Bus& bus() { return bus_; }
    const Main8088Bus& bus() const { return bus_; }

    class Core final : public CpuCore {
    public:
        Core(Main8088Bus& bus,CpuType variant);
        void reset() override;
        void executeOne() override;
        std::uint32_t pc() const override;
        void irq() override;
        void nmi() override;
        CpuType type() const override {return variant_;}
        WORD ip() const {return ip_;}
        WORD cs() const {return segments_[1];}
        WORD flags() const {return flags_;}
        WORD reg(unsigned i) const {return regs_.at(i);}
        bool halted() const {return halted_;}
    private:
        std::uint32_t physical(WORD seg,WORD off) const;
        BYTE fetch8();
        WORD fetch16();
        void interrupt(BYTE vector);
        void push16(WORD value);
        Main8088Bus& bus_;
        CpuType variant_;
        std::array<WORD,8> regs_{};
        std::array<WORD,4> segments_{}; // ES, CS, SS, DS
        WORD ip_=0;
        WORD flags_=0x0002;
        bool halted_=false;
    };
private:
    Main8088Bus bus_;
    Core core_;
};
