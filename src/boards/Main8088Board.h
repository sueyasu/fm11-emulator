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
        WORD pop16();
        struct Operand {
            BYTE reg=0;
            BYTE index=0;
            BYTE segment=0;
            WORD offset=0;
            std::uint32_t address=0;
            bool registerOperand=false;
        };
        Operand decodeModRm();
        BYTE getReg8(unsigned index) const;
        void setReg8(unsigned index,BYTE value);
        BYTE read8(const Operand& op) const;
        WORD read16(const Operand& op) const;
        void write8(const Operand& op,BYTE value);
        void write16(const Operand& op,WORD value);
        void setArithmeticFlags(unsigned lhs,unsigned rhs,unsigned result,unsigned bits,bool subtract);
        void setLogicFlags(unsigned result,unsigned bits);
        bool jumpCondition(unsigned code) const;
        unsigned alu(unsigned operation,unsigned lhs,unsigned rhs,unsigned bits);
        unsigned readOperand(const Operand& op,unsigned bits) const;
        void writeOperand(const Operand& op,unsigned bits,unsigned value);
        unsigned incDec(unsigned lhs,unsigned bits,bool decrement);
        unsigned getReg(unsigned index,unsigned bits) const;
        void setReg(unsigned index,unsigned bits,unsigned value);
        void executeAluModRm(BYTE opcode);
        unsigned shiftRotate(unsigned operation,unsigned value,unsigned bits,unsigned count);
        Main8088Bus& bus_;
        CpuType variant_;
        std::array<WORD,8> regs_{};
        std::array<WORD,4> segments_{}; // ES, CS, SS, DS
        WORD ip_=0;
        WORD flags_=0x0002;
        bool halted_=false;
        unsigned irqInhibit_=0;
    };
private:
    Main8088Bus bus_;
    Core core_;
};
