#pragma once

#include <stdexcept>
#include <utility>
#include <variant>

#include "crossa/runtime/errors/CrossaError.h"

namespace crossa::runtime {

// Identifies the exactly-once terminal state of one native operation.
enum class CrossaStateKind {
    Success,
    Failed,
    Cancelled
};

// Owns either typed success data, a structured failure, or cancellation.
// Static factories prevent invalid combinations of state and payload.
template<typename T>
class CrossaState final {
public:
    // Creates a successful terminal state with typed native data.
    [[nodiscard]] static CrossaState success(T data) {
        return CrossaState(
            CrossaStateKind::Success,
            Storage(std::in_place_index<0>, std::move(data))
        );
    }

    // Creates a failed terminal state with one structured error.
    [[nodiscard]] static CrossaState failed(CrossaError error) {
        if (error.getCode() == CrossaErrorCode::Cancellation) {
            return cancelled();
        }
        return CrossaState(
            CrossaStateKind::Failed,
            Storage(std::in_place_index<1>, std::move(error))
        );
    }

    // Creates a cancelled terminal state without a success payload.
    [[nodiscard]] static CrossaState cancelled() {
        return CrossaState(
            CrossaStateKind::Cancelled,
            Storage(std::in_place_index<2>, std::monostate{})
        );
    }

    // Returns the terminal state category.
    [[nodiscard]] CrossaStateKind getKind() const noexcept {
        return kind_;
    }

    // Returns whether this state contains successful data.
    [[nodiscard]] bool isSuccess() const noexcept {
        return kind_ == CrossaStateKind::Success;
    }

    // Returns whether this state contains a structured failure.
    [[nodiscard]] bool isFailed() const noexcept {
        return kind_ == CrossaStateKind::Failed;
    }

    // Returns whether this operation was cancelled.
    [[nodiscard]] bool isCancelled() const noexcept {
        return kind_ == CrossaStateKind::Cancelled;
    }

    // Returns the typed success data and requires Success.
    [[nodiscard]] const T& getData() const {
        if (!isSuccess()) {
            throw std::logic_error("CrossaState does not contain success data.");
        }
        return std::get<0>(storage_);
    }

    // Moves the typed success data out and requires Success.
    [[nodiscard]] T takeData() {
        if (!isSuccess()) {
            throw std::logic_error("CrossaState does not contain success data.");
        }
        return std::move(std::get<0>(storage_));
    }

    // Returns the structured failure and requires Failed.
    [[nodiscard]] const CrossaError& getError() const {
        if (!isFailed()) {
            throw std::logic_error("CrossaState does not contain an error.");
        }
        return std::get<1>(storage_);
    }

private:
    using Storage = std::variant<T, CrossaError, std::monostate>;

    // Creates one state whose category matches its variant payload.
    CrossaState(CrossaStateKind kind, Storage storage)
        : kind_(kind), storage_(std::move(storage)) {}

    CrossaStateKind kind_;
    Storage storage_;
};

}
