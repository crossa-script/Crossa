#pragma once

#include <cstddef>

namespace crossa::runtime::scheduler {

// Defines bounded worker and queue resources for one runtime scheduler.
// createDefault() derives conservative values without fixing platform policy.
class SchedulerOptions final {
public:
    // Creates validated scheduler options.
    SchedulerOptions(std::size_t workerCount, std::size_t maximumQueuedTasks);

    // Creates conservative options derived from available hardware.
    [[nodiscard]] static SchedulerOptions createDefault();

    // Returns the bounded worker count.
    [[nodiscard]] std::size_t getWorkerCount() const noexcept;

    // Returns the maximum number of queued tasks.
    [[nodiscard]] std::size_t getMaximumQueuedTasks() const noexcept;

private:
    std::size_t workerCount_;
    std::size_t maximumQueuedTasks_;
};

}
