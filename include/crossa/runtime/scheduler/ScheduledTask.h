#pragma once

#include <future>

#include "crossa/runtime/RequestHandle.h"
#include "crossa/runtime/RuntimeValue.h"
#include "crossa/runtime/result/CrossaState.h"

namespace crossa::runtime::scheduler {

// Owns one scheduled operation's cancellation handle and terminal future.
// cancel() and await() expose safe native async lifecycle control.
class ScheduledTask final {
public:
    // Creates one scheduled task from its shared handle and unique future.
    ScheduledTask(
        RequestHandle requestHandle,
        std::future<CrossaState<RuntimeValue>> future
    );

    ScheduledTask(ScheduledTask&& other) noexcept = default;
    ScheduledTask& operator=(ScheduledTask&& other) noexcept = default;
    ScheduledTask(const ScheduledTask&) = delete;
    ScheduledTask& operator=(const ScheduledTask&) = delete;

    // Requests cancellation and returns true only for the first request.
    [[nodiscard]] bool cancel() const noexcept;

    // Returns a copy of the shared cancellation handle.
    [[nodiscard]] RequestHandle getRequestHandle() const noexcept;

    // Waits for and returns the exactly-once terminal state.
    [[nodiscard]] CrossaState<RuntimeValue> await();

private:
    RequestHandle requestHandle_;
    std::future<CrossaState<RuntimeValue>> future_;
};

}
