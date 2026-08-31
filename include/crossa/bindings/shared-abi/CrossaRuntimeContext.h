#pragma once

#include <cstdint>
#include <memory>
#include <mutex>
#include <unordered_map>

#include "crossa/bindings/shared-abi/CrossaAbi.h"
#include "crossa/runtime/RuntimeValue.h"
#include "crossa/runtime/errors/CrossaError.h"

namespace crossa::bindings::sharedabi {

// Owns ABI result handles and keeps native result storage alive for platform views.
class CrossaRuntimeContext final {
public:
    // Creates an empty runtime-owned registry for native result values.
    CrossaRuntimeContext() = default;

    CrossaRuntimeContext(const CrossaRuntimeContext&) = delete;
    CrossaRuntimeContext& operator=(const CrossaRuntimeContext&) = delete;

    // Stores one terminal native value and returns its opaque result handle.
    [[nodiscard]] CrossaResultHandle retainResult(runtime::RuntimeValue value);

    // Releases one result handle and its retained native storage.
    [[nodiscard]] CrossaStatus releaseResult(CrossaResultHandle result);

    // Returns one retained result value or null when the handle is invalid.
    [[nodiscard]] std::shared_ptr<const runtime::RuntimeValue> findResult(
        CrossaResultHandle result
    ) const;

    // Stores one terminal native error and returns its opaque error handle.
    [[nodiscard]] CrossaErrorHandle retainError(runtime::CrossaError error);

    // Releases one error handle and its retained native storage.
    [[nodiscard]] CrossaStatus releaseError(CrossaErrorHandle error);

    // Returns one retained error or null when the handle is invalid.
    [[nodiscard]] std::shared_ptr<const runtime::CrossaError> findError(
        CrossaErrorHandle error
    ) const;

private:
    std::uint64_t nextResult_ = 1;
    mutable std::mutex mutex_;
    std::unordered_map<CrossaResultHandle, std::shared_ptr<const runtime::RuntimeValue>>
        results_;
    std::uint64_t nextError_ = 1;
    std::unordered_map<CrossaErrorHandle, std::shared_ptr<const runtime::CrossaError>>
        errors_;
};

}
