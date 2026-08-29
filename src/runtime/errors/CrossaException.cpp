#include "crossa/runtime/errors/CrossaException.h"

#include <utility>

using namespace std;

namespace crossa::runtime {

    // Creates an internal exception from one immutable structured error.
    CrossaException::CrossaException(CrossaError error)
        : runtime_error(error.format()), error_(std::move(error)) {}

    // Returns the structured native cause.
    const CrossaError& CrossaException::getError() const noexcept {
        return error_;
    }

}
