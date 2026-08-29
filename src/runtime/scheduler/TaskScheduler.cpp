#include "crossa/runtime/scheduler/TaskScheduler.h"

#include <stdexcept>
#include <utility>

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
        for (size_t index = 0; index < options.getWorkerCount(); ++index) {
            workers_.emplace_back([this]() { workerLoop(); });
        }
        log_.debug(
            "Scheduler started: workers=" + to_string(workers_.size()) +
            " queueCapacity=" + to_string(maximumQueuedTasks_)
        );
    }

    // Stops submissions, drains queued work, and joins every worker.
    TaskScheduler::~TaskScheduler() {
        shutdown();
    }

    // Submits a result-producing task and returns its terminal future.
    future<RuntimeValue> TaskScheduler::submit(
        function<RuntimeValue()> task
    ) {
        auto packaged = make_shared<packaged_task<RuntimeValue()>>(
            std::move(task)
        );
        future<RuntimeValue> result = packaged->get_future();
        enqueue([packaged]() { (*packaged)(); });
        return result;
    }

    // Submits fire-and-forget work to the bounded queue.
    void TaskScheduler::submitDetached(function<void()> task) {
        enqueue(std::move(task));
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

    // Stops accepting tasks and joins all workers after draining work.
    void TaskScheduler::shutdown() {
        {
            lock_guard lock(mutex_);
            if (stopping_) {
                return;
            }
            stopping_ = true;
        }
        taskAvailable_.notify_all();
        queueSpaceAvailable_.notify_all();
        for (thread& worker : workers_) {
            if (worker.joinable()) {
                worker.join();
            }
        }
        workers_.clear();
        log_.debug("Scheduler stopped");
    }

    // Adds one task while respecting queue bounds and shutdown state.
    void TaskScheduler::enqueue(function<void()> task) {
        unique_lock lock(mutex_);
        queueSpaceAvailable_.wait(lock, [this]() {
            return stopping_ || tasks_.size() < maximumQueuedTasks_;
        });
        if (stopping_) {
            throw runtime_error("Scheduler is shutting down.");
        }
        tasks_.push_back(std::move(task));
        const size_t queuedTasks = tasks_.size();
        lock.unlock();
        log_.debug(
            "Scheduler task queued: queued=" + to_string(queuedTasks)
        );
        taskAvailable_.notify_one();
    }

    // Processes queued work until shutdown completes.
    void TaskScheduler::workerLoop() {
        {
            lock_guard lock(mutex_);
            workerIds_.insert(this_thread::get_id());
        }
        while (true) {
            function<void()> task;
            size_t queuedTasks = 0;
            size_t activeTasks = 0;
            {
                unique_lock lock(mutex_);
                taskAvailable_.wait(lock, [this]() {
                    return stopping_ || !tasks_.empty();
                });
                if (stopping_ && tasks_.empty()) {
                    workerIds_.erase(this_thread::get_id());
                    return;
                }
                task = std::move(tasks_.front());
                tasks_.pop_front();
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
                task();
            } catch (const exception& error) {
                log_.error("Scheduler task failed: " + string(error.what()));
            } catch (...) {
                log_.error("Scheduler task failed with an unknown error.");
            }

            {
                lock_guard lock(mutex_);
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

}
