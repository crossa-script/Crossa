#include "crossa/runtime/scheduler/ScheduledTask.h"

#include <utility>

using namespace std;

namespace crossa::runtime::scheduler {

    // Creates one scheduled task from its shared handle and unique future.
    ScheduledTask::ScheduledTask(
        RequestHandle requestHandle,
        future<CrossaState<RuntimeValue>> future
    )
        : requestHandle_(std::move(requestHandle)),
          future_(std::move(future)) {}

    // Requests cancellation and returns true only for the first request.
    bool ScheduledTask::cancel() const noexcept {
        return requestHandle_.cancel();
    }

    // Returns a copy of the shared cancellation handle.
    RequestHandle ScheduledTask::getRequestHandle() const noexcept {
        return requestHandle_;
    }

    // Waits for and returns the exactly-once terminal state.
    CrossaState<RuntimeValue> ScheduledTask::await() {
        return future_.get();
    }

}
