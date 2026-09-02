#include "crossa/bindings/shared-abi/CrossaRuntimeContext.h"

#include <limits>
#include <memory>
#include <utility>

using namespace std;

namespace crossa::bindings::sharedabi {

    // Stores one terminal native value and returns its opaque result handle.
    CrossaResultHandle CrossaRuntimeContext::retainResult(runtime::RuntimeValue value) noexcept {
        try {
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
        } catch (...) {
            return 0;
        }
    }

    // Releases one result handle and its retained native storage.
    CrossaStatus CrossaRuntimeContext::releaseResult(CrossaResultHandle result) noexcept {
        try {
            lock_guard lock(mutex_);
            return results_.erase(result) == 1 ? CrossaStatusOk : CrossaStatusInvalidHandle;
        } catch (...) {
            return CrossaStatusInternalError;
        }
    }

    // Returns one retained result value or null when the handle is invalid.
    shared_ptr<const runtime::RuntimeValue> CrossaRuntimeContext::findResult(
        CrossaResultHandle result
    ) const noexcept {
        try {
            lock_guard lock(mutex_);
            const auto found = results_.find(result);
            return found == results_.end() ? nullptr : found->second;
        } catch (...) {
            return nullptr;
        }
    }

    // Stores one terminal native error behind a runtime-local opaque handle.
    CrossaErrorHandle CrossaRuntimeContext::retainError(runtime::CrossaError error) noexcept {
        try {
            lock_guard lock(mutex_);
            if (nextError_ == numeric_limits<CrossaErrorHandle>::max()) {
                return 0;
            }
            const CrossaErrorHandle handle = nextError_++;
            errors_.emplace(handle, make_shared<const runtime::CrossaError>(
                std::move(error)
            ));
            return handle;
        } catch (...) {
            return 0;
        }
    }

    // Releases one retained error and invalidates its opaque handle.
    CrossaStatus CrossaRuntimeContext::releaseError(CrossaErrorHandle error) noexcept {
        try {
            lock_guard lock(mutex_);
            return errors_.erase(error) == 1 ? CrossaStatusOk : CrossaStatusInvalidHandle;
        } catch (...) {
            return CrossaStatusInternalError;
        }
    }

    // Resolves one retained error while preserving its native storage lifetime.
    shared_ptr<const runtime::CrossaError> CrossaRuntimeContext::findError(
        CrossaErrorHandle error
    ) const noexcept {
        try {
            lock_guard lock(mutex_);
            const auto found = errors_.find(error);
            return found == errors_.end() ? nullptr : found->second;
        } catch (...) {
            return nullptr;
        }
    }

}
