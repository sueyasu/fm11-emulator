#pragma once

#include <condition_variable>
#include <chrono>
#include <cstdint>
#include <cstddef>
#include <exception>
#include <functional>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <utility>
#include <vector>

// Lifecycle control for the future main CPU, display CPU and PTM workers.
// This class does NOT make device memory thread-safe. CPU/device ownership
// and shared bus arbitration must be established before starting those workers.
// Each step must be finite and must return at an instruction/event boundary.
class WorkerGroup {
public:
    using Step=std::function<void()>;
    WorkerGroup()=default;
    WorkerGroup(const WorkerGroup&)=delete;
    WorkerGroup& operator=(const WorkerGroup&)=delete;
    ~WorkerGroup() { stop(); }

    void start(Step step) {
        if(!step)throw std::invalid_argument("worker step must not be empty");
        std::lock_guard<std::mutex> lock(mutex_);
        if(stopping_)throw std::logic_error("worker group has been stopped");
        // All workers start in the same paused/running state, with no
        // implicit device ownership or background activity in legacy mode.
        workers_.emplace_back([this,step=std::move(step)] {
            try {
                for(;;) {
                    {
                        std::unique_lock<std::mutex> lock(mutex_);
                        if(paused_) {
                            ++parked_;
                            changed_.notify_all();
                            changed_.wait(lock,[this]{return stopping_ || !paused_;});
                            --parked_;
                            changed_.notify_all();
                        }
                        if(stopping_)return;
                        ++active_;
                    }
                    // No lock is held during guest execution.
                    try {step();}
                    catch(...) {
                        std::lock_guard<std::mutex> lock(mutex_);
                        if(!failure_)failure_=std::current_exception();
                        stopping_=true;
                        --active_;
                        changed_.notify_all();
                        return;
                    }
                    {
                        std::lock_guard<std::mutex> lock(mutex_);
                        --active_;
                        changed_.notify_all();
                    }
                }
            } catch(...) {
                std::lock_guard<std::mutex> lock(mutex_);
                if(!failure_)failure_=std::current_exception();
                stopping_=true;
                changed_.notify_all();
            }
        });
    }

    std::uint64_t wakeGeneration() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return wakeGeneration_;
    }
    void notify() {
        std::lock_guard<std::mutex> lock(mutex_);
        ++wakeGeneration_;
        changed_.notify_all();
    }
    // An interruptible wait inside a worker step. stop/pause also wake it.
    void waitFor(std::chrono::nanoseconds duration,std::uint64_t observed) {
        std::unique_lock<std::mutex> lock(mutex_);
        changed_.wait_for(lock,duration,[this,observed] {
            return stopping_ || paused_ || wakeGeneration_!=observed;
        });
    }
    void pause() {
        std::unique_lock<std::mutex> lock(mutex_);
        if(stopping_)return;
        paused_=true;
        changed_.notify_all();
        // Wait for every worker to finish the current step. The caller can
        // now take a consistent snapshot or change configuration.
        changed_.wait(lock,[this] {
            return stopping_ || (active_==0 && parked_==workers_.size());
        });
    }
    void resume() {
        std::lock_guard<std::mutex> lock(mutex_);
        paused_=false;
        changed_.notify_all();
    }
    void stop() noexcept {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            stopping_=true;
            paused_=false;
            changed_.notify_all();
        }
        // Only the owning/control thread may stop or destroy this group.
        for(auto& worker:workers_)
            if(worker.joinable())worker.join();
        workers_.clear();
    }
    void rethrowWorkerFailure() const {
        std::lock_guard<std::mutex> lock(mutex_);
        if(failure_)std::rethrow_exception(failure_);
    }
private:
    mutable std::mutex mutex_;
    std::condition_variable changed_;
    std::vector<std::thread> workers_;
    std::size_t active_=0,parked_=0;
    bool paused_=false,stopping_=false;
    std::uint64_t wakeGeneration_=0;
    std::exception_ptr failure_;
};
