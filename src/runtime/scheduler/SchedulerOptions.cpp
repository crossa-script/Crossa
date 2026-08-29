#include "crossa/runtime/scheduler/SchedulerOptions.h"

#include <algorithm>
#include <stdexcept>
#include <thread>

using namespace std;

namespace crossa::runtime::scheduler {

    // Creates validated scheduler options.
    SchedulerOptions::SchedulerOptions(
        size_t workerCount,
        size_t maximumQueuedTasks
    )
        : workerCount_(workerCount),
          maximumQueuedTasks_(maximumQueuedTasks) {
        if (workerCount_ == 0 || workerCount_ > 64) {
            throw invalid_argument("Scheduler worker count must be between 1 and 64.");
        }
        if (maximumQueuedTasks_ == 0 || maximumQueuedTasks_ > 65536) {
            throw invalid_argument(
                "Scheduler queue capacity must be between 1 and 65536."
            );
        }
    }

    // Creates conservative options derived from available hardware.
    SchedulerOptions SchedulerOptions::createDefault() {
        const unsigned int detected = thread::hardware_concurrency();
        const size_t available = detected == 0
            ? 2
            : static_cast<size_t>(detected);
        const size_t workers = clamp(available, size_t{2}, size_t{4});
        return SchedulerOptions(workers, 256);
    }

    // Returns the bounded worker count.
    size_t SchedulerOptions::getWorkerCount() const noexcept {
        return workerCount_;
    }

    // Returns the maximum number of queued tasks.
    size_t SchedulerOptions::getMaximumQueuedTasks() const noexcept {
        return maximumQueuedTasks_;
    }

}
