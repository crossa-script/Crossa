#include "crossa/runtime/RequestHandle.h"

#include "crossa/runtime/errors/CrossaException.h"

using namespace std;

namespace crossa::runtime {

    // Creates an active independently cancellable request handle.
    RequestHandle::RequestHandle()
        : state_(make_shared<State>()) {}

    // Requests cancellation and returns true only for the first caller.
    bool RequestHandle::cancel() const noexcept {
        Lifecycle expected = Lifecycle::Active;
        return state_->lifecycle.compare_exchange_strong(
            expected,
            Lifecycle::CancellationRequested,
            memory_order_acq_rel,
            memory_order_acquire
        );
    }

    // Returns whether cancellation has been requested.
    bool RequestHandle::isCancellationRequested() const noexcept {
        return state_->lifecycle.load(memory_order_acquire) ==
            Lifecycle::CancellationRequested;
    }

    // Raises the structured cancellation cause when cancellation was requested.
    void RequestHandle::throwIfCancellationRequested() const {
        if (isCancellationRequested()) {
            throw CrossaException(CrossaError::cancellation());
        }
    }

    // Publishes completion only if cancellation has not already won the race.
    bool RequestHandle::tryComplete() const noexcept {
        Lifecycle expected = Lifecycle::Active;
        return state_->lifecycle.compare_exchange_strong(
            expected,
            Lifecycle::Completed,
            memory_order_acq_rel,
            memory_order_acquire
        );
    }

    bool RequestHandle::completeCancellation() const noexcept {
        Lifecycle expected = Lifecycle::CancellationRequested;
        if (state_->lifecycle.compare_exchange_strong(
            expected,
            Lifecycle::Completed,
            memory_order_acq_rel,
            memory_order_acquire
        )) {
            return true;
        }
        expected = Lifecycle::Active;
        return state_->lifecycle.compare_exchange_strong(
            expected,
            Lifecycle::Completed,
            memory_order_acq_rel,
            memory_order_acquire
        );
    }

}
