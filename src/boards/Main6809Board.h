#pragma once

#include "CpuBoard.h"
#include "Sim09.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

class Main6809Board final : public CpuBoard
{
public:
    explicit Main6809Board(CpuType variant = CpuType::HD6309);

    std::string_view name() const override;
    CpuCore& cpu() override;
    const CpuCore& cpu() const override;
    void reset() override;

    BYTE readCpu(WORD address) const;
    void writeCpu(WORD address, BYTE value);

    BYTE readPhysical(std::uint32_t address) const;
    void writePhysical(std::uint32_t address, BYTE value);

private:
    static constexpr std::size_t kPhysicalMemorySize = 1u << 20;
    static constexpr WORD kMmrPageSize = 0x1000;

    bool isMmrRegister(WORD address) const;
    BYTE readMmrRegister(WORD address) const;
    void writeMmrRegister(WORD address, BYTE value);
    std::uint32_t translateCpuAddress(WORD address) const;

    class Core final : public CSim09, public CpuCore
    {
    public:
        Core(Main6809Board& board, CpuType variant);

        BYTE GetMemByte(WORD address) override;
        void SetMemByte(WORD address, BYTE value) override;

        void reset() override;
        void executeOne() override;
        std::uint32_t pc() const override;
        void irq() override;
        void nmi() override;
        void firq() override;
        CpuType type() const override;

    private:
        Main6809Board& board_;
        CpuType variant_;
    };

    std::vector<BYTE> memory_;
    std::array<std::array<BYTE, 16>, 16> mmr_{};
    std::array<BYTE, 4> mmrControl_{};
    Core core_;
};
