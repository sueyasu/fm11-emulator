#include "WorkerGroup.h"
#include <atomic>
#include <chrono>
#include <stdexcept>
#include <thread>

// Avoid assuming that every host schedules all worker threads within a
// fixed sleep interval (especially under CI load).
template<class Predicate>
bool waitUntil(Predicate done,std::chrono::milliseconds timeout=std::chrono::seconds(2)) {
    const auto deadline=std::chrono::steady_clock::now()+timeout;
    while(!done()) {
        if(std::chrono::steady_clock::now()>=deadline)return false;
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    return true;
}

int main() {
    using namespace std::chrono_literals;
    {
        WorkerGroup workers;
        std::atomic<unsigned> mainSteps{0}, displaySteps{0}, ptmSteps{0};
        workers.start([&]{++mainSteps;});
        workers.start([&]{++displaySteps;});
        workers.start([&]{++ptmSteps;});
        if(!waitUntil([&]{return mainSteps.load()>0 && displaySteps.load()>0 && ptmSteps.load()>0;}))return 1;
        workers.pause();
        const unsigned a=mainSteps,b=displaySteps,c=ptmSteps;
        if(a==0 || b==0 || c==0)return 1;
        std::this_thread::sleep_for(10ms);
        if(mainSteps!=a || displaySteps!=b || ptmSteps!=c)return 2;
        workers.resume();
        if(!waitUntil([&]{return mainSteps.load()>a && displaySteps.load()>b && ptmSteps.load()>c;}))return 3;
        workers.pause();
        workers.stop();
        workers.stop(); // idempotent
    }
    {
        WorkerGroup workers;
        std::atomic<bool> entered{false};
        workers.start([&] {entered=true; throw std::runtime_error("guest worker failure");});
        if(!waitUntil([&]{return entered.load();}))return 4;
        workers.stop();
        try {workers.rethrowWorkerFailure();return 4;}
        catch(const std::runtime_error&) {}
    }
    {
        WorkerGroup workers;
        bool rejected=false;
        try {workers.start({});}
        catch(const std::invalid_argument&) {rejected=true;}
        if(!rejected)return 5;
    }
    {
        WorkerGroup workers;
        std::atomic<unsigned> waits{0};
        workers.start([&] {
            const auto gen=workers.wakeGeneration();
            ++waits;
            workers.waitFor(2s,gen);
        });
        if(!waitUntil([&]{return waits.load()>=1;}))return 6;
        workers.notify();
        if(!waitUntil([&]{return waits.load()>=2;}))return 6;
        // pause and stop must interrupt an otherwise long timed wait.
        workers.pause();
        const auto before=waits.load();
        std::this_thread::sleep_for(5ms);
        if(waits.load()!=before)return 7;
        workers.resume();
        workers.stop();
    }
    return 0;
}
