#pragma once

#include <atomic>
#include <memory>

namespace crossa::runtime {

namespace scheduler {
class TaskScheduler;
}

// Shares one race-safe cancellation signal across scheduler and native work.
// cancel() is idempotent and safe before, during, or after execution.
class RequestHandle final {
public:
    // Creates an active independently cancellable request handle.
    RequestHandle();

    // Requests cancellation and returns true only for the first caller.
    [[nodiscard]] bool cancel() const noexcept;

    // Returns whether cancellation has been requested.
    [[nodiscard]] bool isCancellationRequested() const noexcept;

    // Raises the structured cancellation cause when cancellation was requested.
    void throwIfCancellationRequested() const;

private:
    friend class scheduler::TaskScheduler;

    // Identifies the atomic lifecycle used to linearize cancel and completion.
    enum class Lifecycle {
        Active,
        CancellationRequested,
        Completed
    };

    // Stores the only mutable state shared by operation participants.
    class State final {
    public:
        std::atomic<Lifecycle> lifecycle{Lifecycle::Active};
    };

    // Publishes completion only if cancellation has not already won the race.
    [[nodiscard]] bool tryComplete() const noexcept;

    [[nodiscard]] bool completeCancellation() const noexcept;

    std::shared_ptr<State> state_;
};

}
