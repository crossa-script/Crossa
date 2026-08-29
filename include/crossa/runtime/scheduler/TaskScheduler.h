#pragma once

#include <condition_variable>
#include <cstddef>
#include <deque>
#include <functional>
#include <future>
#include <mutex>
#include <thread>
#include <unordered_set>
#include <vector>

#include "crossa/runtime/RuntimeValue.h"
#include "crossa/runtime/scheduler/SchedulerOptions.h"
#include "crossa/utils/Log.h"

namespace crossa::runtime::scheduler {

// Owns the runtime's bounded worker pool and task queue lifecycle.
// submit(), submitDetached(), waitUntilIdle(), and shutdown() coordinate work.
class TaskScheduler final {
public:
    // Starts a bounded worker pool using validated options.
    TaskScheduler(const SchedulerOptions& options, const utils::Log& log);

    // Stops submissions, drains queued work, and joins every worker.
    ~TaskScheduler();

    TaskScheduler(const TaskScheduler&) = delete;
    TaskScheduler& operator=(const TaskScheduler&) = delete;

    // Submits a result-producing task and returns its terminal future.
    [[nodiscard]] std::future<RuntimeValue> submit(
        std::function<RuntimeValue()> task
    );

    // Submits fire-and-forget work to the bounded queue.
    void submitDetached(std::function<void()> task);

    // Waits until the queue is empty and no worker is executing.
    void waitUntilIdle();

    // Returns whether the caller is one of this scheduler's workers.
    [[nodiscard]] bool isWorkerThread() const;

    // Stops accepting tasks and joins all workers after draining work.
    void shutdown();

private:
    // Adds one task while respecting queue bounds and shutdown state.
    void enqueue(std::function<void()> task);

    // Processes queued work until shutdown completes.
    void workerLoop();

    const utils::Log& log_;
    std::size_t maximumQueuedTasks_;
    mutable std::mutex mutex_;
    std::condition_variable taskAvailable_;
    std::condition_variable queueSpaceAvailable_;
    std::condition_variable idle_;
    std::deque<std::function<void()>> tasks_;
    std::vector<std::thread> workers_;
    std::unordered_set<std::thread::id> workerIds_;
    std::size_t activeTasks_;
    bool stopping_;
};

}
