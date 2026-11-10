#pragma once

#include "CpuBoard.h"
#include "MainBoard.h"
#include "SimZ80.h"
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <vector>

// The optional Z80 CPU card shares the main 6809 physical RAM and system bus.
// FC00-FFFF is card/system I/O rather than ordinary Z80 DRAM.
class MainZ80Board final : public CpuBoard {
public:
    MainZ80Board();
    std::string_view name() const override {return "mainz80";}
    CpuCore& cpu() override {return core_;}
    const CpuCore& cpu() const override {return core_;}
    void reset() override;
    void setMainBoard(std::shared_ptr<MainBoard> board) {mainBoard_=std::move(board);}
    void setPhysicalMemory(std::shared_ptr<std::vector<BYTE>> memory) {
        if(!memory || memory->size()!=(1u<<20))
            throw std::invalid_argument("invalid Z80 physical RAM");
        memory_=std::move(memory);
    }
    BYTE readMemory(WORD address) const;
    void writeMemory(WORD address,BYTE value);
    BYTE readIo(WORD address) const;
    void writeIo(WORD address,BYTE value);
    BYTE bank() const {return bank_;}
private:
    class Core final : public CSimZ80, public CpuCore {
    public:
        explicit Core(MainZ80Board& board):board_(board){}
        BYTE GetMem(WORD addr) override {return board_.readMemory(addr);}
        void SetMem(WORD addr,BYTE data) override {board_.writeMemory(addr,data);}
        BYTE GetIO(BYTE port) override {return board_.readIo(port);}
        void SetIO(BYTE port,BYTE data) override {board_.writeIo(port,data);}
        void reset() override {regs=ZREGS{};instructionCycles_=0;}
        void executeOne() override {z80emu();}
        std::uint32_t pc() const override {return regs.PC;}
        void irq() override {z80irq();}
        void nmi() override {z80nmi();}
        CpuType type() const override {return CpuType::Z80;}
        std::uint64_t lastExecutionCycles() const override {return LastInstructionCycles();}
    private:
        MainZ80Board& board_;
    };
    std::shared_ptr<std::vector<BYTE>> memory_ =
        std::make_shared<std::vector<BYTE>>(1u<<20,0);
    std::shared_ptr<MainBoard> mainBoard_;
    BYTE bank_=0;
    Core core_;
};
