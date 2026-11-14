#include "WorkerGroup.h"
#include <atomic>
#include <chrono>
#include <stdexcept>
#include <thread>

int main() {
    using namespace std::chrono_literals;
    {
        WorkerGroup workers;
        std::atomic<unsigned> mainSteps{0}, displaySteps{0}, ptmSteps{0};
        workers.start([&]{++mainSteps;});
        workers.start([&]{++displaySteps;});
        workers.start([&]{++ptmSteps;});
        std::this_thread::sleep_for(20ms);
        workers.pause();
        const unsigned a=mainSteps,b=displaySteps,c=ptmSteps;
        if(a==0 || b==0 || c==0)return 1;
        std::this_thread::sleep_for(10ms);
        if(mainSteps!=a || displaySteps!=b || ptmSteps!=c)return 2;
        workers.resume();
        std::this_thread::sleep_for(10ms);
        workers.pause();
        if(mainSteps<=a || displaySteps<=b || ptmSteps<=c)return 3;
        workers.stop();
        workers.stop(); // idempotent
    }
    {
        WorkerGroup workers;
        workers.start([] {throw std::runtime_error("guest worker failure");});
        // pause() either observes the failure or parks the worker before
        // it executes; allow a scheduler interval for the worker to run.
        std::this_thread::sleep_for(10ms);
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
    return 0;
}
