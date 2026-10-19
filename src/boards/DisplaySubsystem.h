#pragma once

// Physical display/sub-CPU subsystem on the FM-11 main board.
//
// At this stage this object owns only the HALT request latch. The sub CPU,
// its RAM/VRAM mapping and display controller will be integrated here once
// their hardware-facing behavior is implemented. HALT is deliberately a
// request, not a claim that an emulated sub CPU has stopped.
class DisplaySubsystem {
public:
    void reset() {haltRequested_=false;}
    void requestHalt() {haltRequested_=true;}
    bool haltRequested() const {return haltRequested_;}

private:
    bool haltRequested_=false;
};
