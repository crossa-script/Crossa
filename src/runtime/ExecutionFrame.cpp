#include "crossa/runtime/ExecutionFrame.h"

#include <utility>

using namespace std;

namespace crossa::runtime {

    // Creates a frame that propagates one native cancellation handle.
    ExecutionFrame::ExecutionFrame(RequestHandle requestHandle)
        : requestHandle_(std::move(requestHandle)) {}

    // Declares a value in the current frame and rejects duplicate names.
    bool ExecutionFrame::declare(const string& name, RuntimeValue value) {
        return values_.emplace(name, std::move(value)).second;
    }

    // Resolves a value in the current frame.
    const RuntimeValue* ExecutionFrame::resolve(const string& name) const noexcept {
        const auto iterator = values_.find(name);
        return iterator == values_.end() ? nullptr : &iterator->second;
    }

    // Returns the request handle inherited by work in this frame.
    const RequestHandle& ExecutionFrame::getRequestHandle() const noexcept {
        return requestHandle_;
    }

}
