#pragma once

#include <condition_variable>
#include <cstddef>
#include <deque>
#include <functional>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "crossa/runtime/RequestHandle.h"
#include "crossa/runtime/RuntimeValue.h"
#include "crossa/runtime/result/CrossaState.h"
#include "crossa/runtime/scheduler/ScheduledTask.h"
#include "crossa/runtime/scheduler/SchedulerOptions.h"
#include "crossa/utils/Log.h"

namespace crossa::runtime::scheduler {

// Owns the runtime's bounded worker pool, cancellation, and task lifecycle.
// submit(), submitDetached(), waitUntilIdle(), and shutdown() coordinate work.
class TaskScheduler final {
public:
    // Starts a bounded worker pool using validated options.
    TaskScheduler(const SchedulerOptions& options, const utils::Log& log);

    // Cancels outstanding work and joins every worker.
    ~TaskScheduler();

    TaskScheduler(const TaskScheduler&) = delete;
    TaskScheduler& operator=(const TaskScheduler&) = delete;

    // Submits a result-producing task with a cancellable terminal state.
    [[nodiscard]] ScheduledTask submit(
        std::function<RuntimeValue(const RequestHandle&)> task
    );

    // Submits fire-and-forget work and returns its cancellation handle.
    [[nodiscard]] RequestHandle submitDetached(
        std::function<void(const RequestHandle&)> task
    );

    // Waits until the queue is empty and no worker is executing.
    void waitUntilIdle();

    // Returns whether the caller is one of this scheduler's workers.
    [[nodiscard]] bool isWorkerThread() const;

    // Stops submissions, cancels outstanding work, and joins all workers.
    void shutdown();

private:
    // Stores one queue entry with the handle used during shutdown cancellation.
    struct ScheduledWork final {
        std::function<void()> task;
        RequestHandle requestHandle;
    };

    // Adds one task while respecting queue bounds and shutdown state.
    void enqueue(ScheduledWork work);

    // Converts one native task into exactly one CrossaState terminal result.
    [[nodiscard]] static CrossaState<RuntimeValue> runTask(
        const std::function<RuntimeValue(const RequestHandle&)>& task,
        const RequestHandle& requestHandle
    );

    // Processes queued work until shutdown completes.
    void workerLoop();

    // Joins every worker that was started successfully.
    void joinWorkers() noexcept;

    const utils::Log& log_;
    std::size_t maximumQueuedTasks_;
    mutable std::mutex mutex_;
    std::condition_variable taskAvailable_;
    std::condition_variable queueSpaceAvailable_;
    std::condition_variable idle_;
    std::deque<ScheduledWork> tasks_;
    std::vector<std::thread> workers_;
    std::unordered_set<std::thread::id> workerIds_;
    std::unordered_map<std::thread::id, RequestHandle> activeHandles_;
    std::size_t activeTasks_;
    bool stopping_;
};

}
