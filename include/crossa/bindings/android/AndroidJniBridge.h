#pragma once

#include <jni.h>

#include "crossa/bindings/shared-abi/CrossaAbi.h"

namespace crossa::bindings::android {

// Registers the reusable Android JNI adapter for one generated Crossa runtime.
class AndroidJniBridge final {
public:
    using RuntimeCreator = CrossaRuntimeHandle (*)(const char*, size_t);

    // Caches JVM metadata and registers every generated bridge native method.
    [[nodiscard]] static bool initialize(
        JavaVM* javaVm,
        JNIEnv* environment,
        const char* bridgeClassName,
        RuntimeCreator runtimeCreator
    ) noexcept;
};

}
