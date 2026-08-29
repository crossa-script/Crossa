#pragma once

#include <string>
#include <unordered_map>

#include "crossa/runtime/RequestHandle.h"
#include "crossa/runtime/RuntimeValue.h"

namespace crossa::runtime {

// Stores parameter and local runtime values for one function invocation.
// declare() and resolve() keep frame ownership explicit during execution.
class ExecutionFrame final {
public:
    // Creates a frame that propagates one native cancellation handle.
    explicit ExecutionFrame(
        RequestHandle requestHandle = RequestHandle(),
        const ExecutionFrame* parent = nullptr
    );

    // Declares a value in the current frame and rejects duplicate names.
    [[nodiscard]] bool declare(
        const std::string& name,
        RuntimeValue value
    );

    // Resolves a value in the current frame.
    [[nodiscard]] const RuntimeValue* resolve(
        const std::string& name
    ) const noexcept;

    // Returns the request handle inherited by work in this frame.
    [[nodiscard]] const RequestHandle& getRequestHandle() const noexcept;

private:
    RequestHandle requestHandle_;
    const ExecutionFrame* parent_;
    std::unordered_map<std::string, RuntimeValue> values_;
};

}
