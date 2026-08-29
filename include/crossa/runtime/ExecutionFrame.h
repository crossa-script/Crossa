#pragma once

#include <string>
#include <unordered_map>

#include "crossa/runtime/RuntimeValue.h"

namespace crossa::runtime {

// Stores parameter and local runtime values for one function invocation.
// declare() and resolve() keep frame ownership explicit during execution.
class ExecutionFrame final {
public:
    // Declares a value in the current frame and rejects duplicate names.
    [[nodiscard]] bool declare(
        const std::string& name,
        RuntimeValue value
    );

    // Resolves a value in the current frame.
    [[nodiscard]] const RuntimeValue* resolve(
        const std::string& name
    ) const noexcept;

private:
    std::unordered_map<std::string, RuntimeValue> values_;
};

}
