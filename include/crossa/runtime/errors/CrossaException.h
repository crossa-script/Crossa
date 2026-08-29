#pragma once

#include <stdexcept>

#include "crossa/runtime/errors/CrossaError.h"

namespace crossa::runtime {

// Propagates a structured CrossaError inside native implementation boundaries.
// Public ABI layers consume the error value and never expose this exception.
class CrossaException final : public std::runtime_error {
public:
    // Creates an internal exception from one immutable structured error.
    explicit CrossaException(CrossaError error);

    // Returns the structured native cause.
    [[nodiscard]] const CrossaError& getError() const noexcept;

private:
    CrossaError error_;
};

}
