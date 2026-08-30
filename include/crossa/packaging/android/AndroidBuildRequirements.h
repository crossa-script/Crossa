#pragma once

#include <string>

namespace crossa::packaging::android {

// Defines Android build requirements shared by generation and environment checks.
// compileSdkVersion(), ndkVersion(), cmakeMinimumVersion(), and javaToolchainVersion() expose the pinned values.
class AndroidBuildRequirements final {
public:
    // Returns the Android SDK platform API required by generated AAR builds.
    [[nodiscard]] static int compileSdkVersion() noexcept;

    // Returns the side-by-side Android NDK version required by generated AAR builds.
    [[nodiscard]] static std::string ndkVersion();

    // Returns the minimum CMake version declared by generated native build files.
    [[nodiscard]] static std::string cmakeMinimumVersion();

    // Returns the Java toolchain major version required by generated Gradle builds.
    [[nodiscard]] static int javaToolchainVersion() noexcept;

    // Returns the Android Gradle Plugin version used by generated AAR builds.
    [[nodiscard]] static std::string androidGradlePluginVersion();

    // Returns the Kotlin Android plugin version used by generated AAR builds.
    [[nodiscard]] static std::string kotlinAndroidPluginVersion();
};

}
