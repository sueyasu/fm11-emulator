#include "Intel8088BusEventSchedule.h"
#include <vector>
#include <iostream>

int main() {
    using E=Intel8088BusEvents::Event;
    using K=Intel8088BusEvents::Kind;
    using S=Intel8088BusEventSchedule::Status;
    const std::vector<E> events{
        {K::Fetch,0x10000,0x8b,0},
        {K::Fetch,0x10001,0x07,1},
        {K::MemoryRead,0x200,0x34,2},
        {K::MemoryRead,0x201,0x12,3}
    };
    Intel8088BusEventSchedule schedule;
    // A missing schedule must not pretend bus event timing was known.
    auto r=schedule.evaluate(events,17,{});
    if(r.status!=S::CountMismatch || r.bus.elapsed!=0)return 1;
    r=schedule.evaluate(events,17,{5});
    if(r.status!=S::CountMismatch || r.bus.elapsed!=0)return 2;
    r=schedule.evaluate(events,17,{5,8});
    if(r.status!=S::Overlap || r.bus.elapsed!=0)return 3;
    r=schedule.evaluate(events,17,{5,15});
    if(r.status!=S::OutOfRange || r.bus.elapsed!=0)return 4;
    r=schedule.evaluate(events,17,{5,9});
    if(r.status!=S::Valid || r.operandEvents!=2 ||
       r.prefetchGrants!=2 || r.bus.elapsed!=17 ||
       r.bus.operand!=8 || r.bus.prefetch!=8 ||
       r.bus.interrupted!=1 || r.bus.partial!=0)return 5;

    // A subsequent event-free NOP continues the same prefetch phase.
    const std::vector<E> nop{{K::Fetch,0x10002,0x90,0}};
    r=schedule.evaluate(nop,3,{});
    if(r.status!=S::Valid || r.prefetchGrants!=0 ||
       r.bus.elapsed!=20 || r.bus.partial!=3)return 6;
    r=schedule.evaluate(nop,3,{});
    if(r.status!=S::Valid || r.prefetchGrants!=1 ||
       r.bus.elapsed!=23 || r.bus.partial!=2)return 7;

    // Invalid input must not advance a previously valid timeline.
    r=schedule.evaluate(events,17,{6,7});
    if(r.status!=S::Overlap)return 8;
    r=schedule.evaluate(nop,3,{});
    if(r.bus.elapsed!=26 || r.bus.partial!=1)return 9;

    schedule.flush();
    r=schedule.evaluate(nop,3,{});
    if(r.bus.elapsed!=29 || r.bus.partial!=3 ||
       r.bus.interrupted!=2)return 10;
    schedule.reset();
    r=schedule.lastResult();
    if(r.status!=S::Unspecified || r.bus.elapsed)return 11;
    return 0;
}
