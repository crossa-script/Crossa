#include "crossa/runtime/scheduler/TaskScheduler.h"

#include <exception>
#include <future>
#include <stdexcept>
#include <utility>

#include "crossa/runtime/errors/CrossaException.h"

using namespace std;

namespace crossa::runtime::scheduler {

    // Starts a bounded worker pool using validated options.
    TaskScheduler::TaskScheduler(
        const SchedulerOptions& options,
        const utils::Log& log
    )
        : log_(log),
          maximumQueuedTasks_(options.getMaximumQueuedTasks()),
          activeTasks_(0),
          stopping_(false) {
        workers_.reserve(options.getWorkerCount());
        try {
            for (size_t index = 0; index < options.getWorkerCount(); ++index) {
                workers_.emplace_back([this]() { workerLoop(); });
            }
        } catch (...) {
            {
                lock_guard lock(mutex_);
                stopping_ = true;
            }
            taskAvailable_.notify_all();
            joinWorkers();
            throw;
        }
        log_.debug(
            "Scheduler started: workers=" + to_string(workers_.size()) +
            " queueCapacity=" + to_string(maximumQueuedTasks_)
        );
    }

    // Cancels outstanding work and joins every worker.
    TaskScheduler::~TaskScheduler() {
        shutdown();
    }

    // Submits a result-producing task with a cancellable terminal state.
    ScheduledTask TaskScheduler::submit(
        function<RuntimeValue(const RequestHandle&)> task
    ) {
        RequestHandle requestHandle;
        auto packaged = make_shared<packaged_task<CrossaState<RuntimeValue>()>>(
            [task = std::move(task), requestHandle]() {
                return runTask(task, requestHandle);
            }
        );
        future<CrossaState<RuntimeValue>> result = packaged->get_future();
        enqueue(ScheduledWork{
            [packaged]() { (*packaged)(); },
            requestHandle
        });
        return ScheduledTask(requestHandle, std::move(result));
    }

    // Submits fire-and-forget work and returns its cancellation handle.
    RequestHandle TaskScheduler::submitDetached(
        function<void(const RequestHandle&)> task
    ) {
        RequestHandle requestHandle;
        enqueue(ScheduledWork{
            [this, task = std::move(task), requestHandle]() {
                CrossaState<RuntimeValue> state = runTask(
                    [&task](const RequestHandle& handle) {
                        task(handle);
                        return RuntimeValue::createUnit();
                    },
                    requestHandle
                );
                if (state.isFailed()) {
                    log_.error(
                        "Detached scheduler task failed: " +
                        state.getError().format()
                    );
                } else if (state.isCancelled()) {
                    log_.debug("Detached scheduler task cancelled");
                }
            },
            requestHandle
        });
        return requestHandle;
    }

    // Submits one result-producing task and delivers its terminal state on a worker.
    RequestHandle TaskScheduler::submitWithCompletion(
        function<RuntimeValue(const RequestHandle&)> task,
        function<void(CrossaState<RuntimeValue>)> completion
    ) {
        RequestHandle requestHandle;
        enqueue(ScheduledWork{
            [this, task = std::move(task), completion = std::move(completion),
             requestHandle]() mutable {
                CrossaState<RuntimeValue> state = runTask(task, requestHandle);
                try {
                    completion(std::move(state));
                } catch (const exception& error) {
                    log_.error("Scheduler completion callback failed: " +
                        string(error.what()));
                } catch (...) {
                    log_.error("Scheduler completion callback failed.");
                }
            },
            requestHandle
        });
        return requestHandle;
    }

    // Waits until the queue is empty and no worker is executing.
    void TaskScheduler::waitUntilIdle() {
        unique_lock lock(mutex_);
        idle_.wait(lock, [this]() {
            return tasks_.empty() && activeTasks_ == 0;
        });
    }

    // Returns whether the caller is one of this scheduler's workers.
    bool TaskScheduler::isWorkerThread() const {
        lock_guard lock(mutex_);
        return workerIds_.contains(this_thread::get_id());
    }

    // Stops submissions, cancels outstanding work, and joins all workers.
    void TaskScheduler::shutdown() {
        {
            lock_guard lock(mutex_);
            if (stopping_) {
                return;
            }
            stopping_ = true;
            for (const ScheduledWork& work : tasks_) {
                (void)work.requestHandle.cancel();
            }
            for (const auto& [workerId, requestHandle] : activeHandles_) {
                (void)workerId;
                (void)requestHandle.cancel();
            }
        }
        taskAvailable_.notify_all();
        queueSpaceAvailable_.notify_all();
        joinWorkers();
        log_.debug("Scheduler stopped");
    }

    // Adds one task while respecting queue bounds and shutdown state.
    void TaskScheduler::enqueue(ScheduledWork work) {
        unique_lock lock(mutex_);
        queueSpaceAvailable_.wait(lock, [this]() {
            return stopping_ || tasks_.size() < maximumQueuedTasks_;
        });
        if (stopping_) {
            throw CrossaException(
                CrossaError::runtime("Scheduler is shutting down.")
            );
        }
        tasks_.push_back(std::move(work));
        const size_t queuedTasks = tasks_.size();
        lock.unlock();
        log_.debug(
            "Scheduler task queued: queued=" + to_string(queuedTasks)
        );
        taskAvailable_.notify_one();
    }

    // Converts one native task into exactly one CrossaState terminal result.
    CrossaState<RuntimeValue> TaskScheduler::runTask(
        const function<RuntimeValue(const RequestHandle&)>& task,
        const RequestHandle& requestHandle
    ) {
        try {
            requestHandle.throwIfCancellationRequested();
            RuntimeValue value = task(requestHandle);
            if (!requestHandle.tryComplete()) {
                return CrossaState<RuntimeValue>::cancelled();
            }
            return CrossaState<RuntimeValue>::success(std::move(value));
        } catch (const CrossaException& error) {
            if (error.getError().getCode() == CrossaErrorCode::Cancellation) {
                (void)requestHandle.tryComplete();
                return CrossaState<RuntimeValue>::cancelled();
            }
            if (!requestHandle.tryComplete()) {
                return CrossaState<RuntimeValue>::cancelled();
            }
            return CrossaState<RuntimeValue>::failed(error.getError());
        } catch (const exception& error) {
            if (!requestHandle.tryComplete()) {
                return CrossaState<RuntimeValue>::cancelled();
            }
            return CrossaState<RuntimeValue>::failed(
                CrossaError::runtime(error.what())
            );
        } catch (...) {
            if (!requestHandle.tryComplete()) {
                return CrossaState<RuntimeValue>::cancelled();
            }
            return CrossaState<RuntimeValue>::failed(
                CrossaError::runtime("Unknown native scheduler failure.")
            );
        }
    }

    // Processes queued work until shutdown completes.
    void TaskScheduler::workerLoop() {
        const thread::id workerId = this_thread::get_id();
        {
            lock_guard lock(mutex_);
            workerIds_.insert(workerId);
        }
        while (true) {
            ScheduledWork work;
            size_t queuedTasks = 0;
            size_t activeTasks = 0;
            {
                unique_lock lock(mutex_);
                taskAvailable_.wait(lock, [this]() {
                    return stopping_ || !tasks_.empty();
                });
                if (stopping_ && tasks_.empty()) {
                    workerIds_.erase(workerId);
                    return;
                }
                work = std::move(tasks_.front());
                tasks_.pop_front();
                activeHandles_.insert_or_assign(
                    workerId,
                    work.requestHandle
                );
                ++activeTasks_;
                queuedTasks = tasks_.size();
                activeTasks = activeTasks_;
                queueSpaceAvailable_.notify_one();
            }

            log_.debug(
                "Scheduler task started: queued=" +
                to_string(queuedTasks) + " active=" +
                to_string(activeTasks)
            );
            try {
                work.task();
            } catch (const exception& error) {
                log_.error("Scheduler task failed: " + string(error.what()));
            } catch (...) {
                log_.error("Scheduler task failed with an unknown error.");
            }

            {
                lock_guard lock(mutex_);
                activeHandles_.erase(workerId);
                --activeTasks_;
                queuedTasks = tasks_.size();
                activeTasks = activeTasks_;
                log_.debug(
                    "Scheduler task completed: queued=" +
                    to_string(queuedTasks) + " active=" +
                    to_string(activeTasks)
                );
                if (tasks_.empty() && activeTasks_ == 0) {
                    idle_.notify_all();
                }
            }
        }
    }

    // Joins every worker that was started successfully.
    void TaskScheduler::joinWorkers() noexcept {
        for (thread& worker : workers_) {
            if (worker.joinable()) {
                worker.join();
            }
        }
        workers_.clear();
    }

}
