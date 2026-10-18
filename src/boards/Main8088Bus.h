#pragma once
#include "portable_types.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <vector>

// Hardware-facing bus for the optional FM-11 8088 CPU card.
// The execution core is deliberately separate.  The original Sim86 maps
// a 20-bit memory space, a top 4-KiB boot ROM and a 16-bit I/O space.
class Main8088Bus {
public:
    static constexpr std::size_t kMemorySize = 0x100000;
    static constexpr std::uint32_t kRomBase = 0xff000;
    static constexpr std::size_t kRomSize = 0x1000;
    using Rom = std::array<BYTE,kRomSize>;
    using IoReader = std::function<BYTE(WORD)>;
    using IoWriter = std::function<void(WORD,BYTE)>;

    Main8088Bus();
    void reset();
    void loadBootRom(const Rom& rom);
    BYTE readMemory(std::uint32_t address) const;
    void writeMemory(std::uint32_t address, BYTE value);
    BYTE readIo(WORD port) const;
    void writeIo(WORD port, BYTE value);
    void setSystemIo(IoReader reader, IoWriter writer);
    BYTE readRam(std::uint32_t address) const;
    // 8259A PIC on the 8088 card: ports 00h/01h.
    void requestIrq(unsigned line);
    bool interruptPending() const;
    BYTE acknowledgeInterrupt();
    BYTE interruptMask() const { return picMask_; }
    struct DmaChannel { WORD address=0, baseAddress=0, count=0, baseCount=0; BYTE mode=0, bank=0; };
    const DmaChannel& dmaChannel(unsigned ch) const { return dmaChannels_.at(ch); }
    BYTE dmaMask() const { return dmaMask_; }
    using DmaRead = std::function<BYTE()>;
    using DmaWrite = std::function<void(BYTE)>;
    void setDmaDevice(unsigned channel,DmaRead reader,DmaWrite writer);
    // External level-sensitive DREQ input, independent of software requests.
    void setDmaRequest(unsigned channel,bool levelHigh);
    // Execute one requested DMA byte; returns false if no transfer can run.
    bool serviceDma();

private:
    std::vector<BYTE> memory_;
    Rom bootRom_{};
    // The original FM-11 Sim86 uses the CPU-card SRAM for FCxx port accesses.
    std::array<BYTE,0x100> cardIoRam_{};
    BYTE picMask_=0;
    BYTE picRequest_=0;
    BYTE picInService_=0;
    BYTE picVectorBase_=0x08;
    BYTE picReadIsr_=0;
    BYTE picInitStep_=0;
    bool picAutoEoi_=false;
    bool picNeedsIcw4_=false;
    bool picSingle_=false;
    std::array<DmaChannel,4> dmaChannels_{};
    BYTE dmaPriorityBase_=0;
    BYTE dmaExternalRequest_=0, dmaExternalDriven_=0, dmaBlockActive_=0;
    BYTE activeDmaRequests() const;
    BYTE dmaCommand_=0, dmaMask_=0, dmaStatus_=0, dmaFlip_=0, dmaRequest_=0;
    std::array<DmaRead,4> dmaReaders_{};
    std::array<DmaWrite,4> dmaWriters_{};
    void resetDma();
    BYTE readDma(WORD port) const;
    void writeDma(WORD port,BYTE value);
    IoReader systemReader_;
    IoWriter systemWriter_;
};
