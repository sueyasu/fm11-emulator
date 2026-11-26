#pragma once
#include "portable_types.h"
#include "FloppyDevice.h"
#include "MainBoard.h"
#include "RomFiles.h"
#include <array>
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <vector>
#include <memory>
#include <stdexcept>
#include <utility>

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
    void loadBootRomFile(const std::string& path) {loadBootRom(RomFiles::load<kRomSize>(path));}
    void setMainBoard(std::shared_ptr<MainBoard> board) {
        mainBoard_=std::move(board);
        setFloppyDevice(mainBoard_->floppyDevice());
    }
    void setFloppyDevice(std::shared_ptr<FloppyDevice> device) {
        floppy_=std::move(device);
        setDmaDevice(0,[this](){return floppyReadData();},
                       [this](BYTE value){floppyWriteData(value);});
        setDmaDevice(1,[this](){return floppyReadData();},
                       [this](BYTE value){floppyWriteData(value);});
    }
    void serviceTimedFloppy(); // elapsed Type-II FDC events
    void loadFloppy2d(const std::vector<BYTE>& image);
    void loadFloppy2hd(const std::vector<BYTE>& image);
    std::vector<BYTE> floppy2hdImage() const { return floppy_->image2hd(); }
    std::vector<BYTE> floppyImage() const { return floppy_->image2d(); }
    bool floppyDirty() const { return floppy_->dirty2d(); }
    void clearFloppyDirty() { floppy_->clearDirty2d(); }
    // 20-bit RAM decoder mirrors absent addresses into populated memory.
    // The upper 4-KiB reset-ROM overlay remains unaffected.
    void setRamKiB(std::size_t kib);
    std::size_t ramKiB() const {return installedRamBytes_/1024;}
    std::shared_ptr<std::vector<BYTE>> physicalMemory() const {return memory_;}
    void setPhysicalMemory(std::shared_ptr<std::vector<BYTE>> memory) {
        if(!memory || memory->size()!=kMemorySize)
            throw std::invalid_argument("invalid shared physical memory");
        memory_=std::move(memory);
    }
    void setMemoryAccessObserver(std::function<void(std::uint32_t,bool,BYTE)> observer) { memoryAccessObserver_=std::move(observer); }
    BYTE readMemory(std::uint32_t address) const;
    BYTE peekMemory(std::uint32_t address) const {return readMemoryUnobserved(address);}
    void writeMemory(std::uint32_t address, BYTE value);
    BYTE readIo(WORD port) const;
    void writeIo(WORD port, BYTE value);
    void setSystemIo(IoReader reader, IoWriter writer);
    BYTE readRam(std::uint32_t address) const;
    // Test-only flat 1-MiB address space, including the reset-ROM region.
    // No effect on ordinary machine execution unless explicitly enabled.
    void enableConformanceRam() {conformanceRam_=true;}
    void clearConformanceRam() {
        std::fill(memory_->begin(),memory_->end(),BYTE(0));
    }
    // 8259A PIC on the 8088 card: ports 00h/01h.
    void requestIrq(unsigned line);
    // Sample main-board IRQ lines as edges into the on-card 8259.
    void refreshSystemIrq();
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
    BYTE readMemoryUnobserved(std::uint32_t address) const;
    void writeMemoryUnobserved(std::uint32_t address, BYTE value);
    mutable std::function<void(std::uint32_t,bool,BYTE)> memoryAccessObserver_;
    std::shared_ptr<std::vector<BYTE>> memory_;
    std::size_t installedRamBytes_=kMemorySize;
    std::size_t decodedRamAddress(std::uint32_t address) const {
        return static_cast<std::size_t>(address&0xfffff)%installedRamBytes_;
    }
    bool conformanceRam_=false;
    Rom bootRom_{};
    // The original FM-11 Sim86 uses the CPU-card SRAM for FCxx port accesses.
    std::array<BYTE,0x100> cardIoRam_{};
    BYTE systemIrqPrevious_=0;
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
    BYTE dmaPriorityBase_=0, dmaTemporary_=0;
    BYTE dmaExternalRequest_=0, dmaExternalDriven_=0, dmaBlockActive_=0;
    BYTE activeDmaRequests() const;
    BYTE dmaCommand_=0, dmaMask_=0, dmaStatus_=0, dmaFlip_=0, dmaRequest_=0;
    std::array<DmaRead,4> dmaReaders_{};
    std::array<DmaWrite,4> dmaWriters_{};
    std::shared_ptr<MainBoard> mainBoard_;
    bool timedTypeIIDispatch_=false;
    std::shared_ptr<FloppyDevice> floppy_=std::make_shared<FloppyDevice>();
    void floppyCommand(BYTE command,unsigned channel);
    BYTE floppyReadData();
    void floppyWriteData(BYTE value);
    void resetDma();
    BYTE readDma(WORD port) const;
    void writeDma(WORD port,BYTE value);
    IoReader systemReader_;
    IoWriter systemWriter_;
};
