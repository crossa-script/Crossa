#include "crossa/bindings/shared-abi/CrossaRuntimeContext.h"

#include <limits>
#include <memory>
#include <utility>

using namespace std;

namespace crossa::bindings::sharedabi {

    // Stores one terminal native value and returns its opaque result handle.
    CrossaResultHandle CrossaRuntimeContext::retainResult(runtime::RuntimeValue value) {
        lock_guard lock(mutex_);
        if (nextResult_ == numeric_limits<CrossaResultHandle>::max()) {
            return 0;
        }
        const CrossaResultHandle result = nextResult_++;
        results_.emplace(
            result,
            make_shared<const runtime::RuntimeValue>(std::move(value))
        );
        return result;
    }

    // Releases one result handle and its retained native storage.
    CrossaStatus CrossaRuntimeContext::releaseResult(CrossaResultHandle result) {
        lock_guard lock(mutex_);
        return results_.erase(result) == 1 ? CrossaStatusOk : CrossaStatusInvalidHandle;
    }

    // Returns one retained result value or null when the handle is invalid.
    shared_ptr<const runtime::RuntimeValue> CrossaRuntimeContext::findResult(
        CrossaResultHandle result
    ) const {
        lock_guard lock(mutex_);
        const auto found = results_.find(result);
        return found == results_.end() ? nullptr : found->second;
    }

}
