#include "crossa/packaging/android/AndroidBuildRequirements.h"

using namespace std;

namespace crossa::packaging::android {

    // Returns the Android SDK platform API required by generated AAR builds.
    int AndroidBuildRequirements::compileSdkVersion() noexcept {
        return 35;
    }

    // Returns the side-by-side Android NDK version required by generated AAR builds.
    string AndroidBuildRequirements::ndkVersion() {
        return "28.1.13356709";
    }

    // Returns the minimum CMake version declared by generated native build files.
    string AndroidBuildRequirements::cmakeMinimumVersion() {
        return "3.22.1";
    }

    // Returns the Java toolchain major version required by generated Gradle builds.
    int AndroidBuildRequirements::javaToolchainVersion() noexcept {
        return 17;
    }

    // Returns the Android Gradle Plugin version used by generated AAR builds.
    string AndroidBuildRequirements::androidGradlePluginVersion() {
        return "8.5.1";
    }

    // Returns the Kotlin Android plugin version used by generated AAR builds.
    string AndroidBuildRequirements::kotlinAndroidPluginVersion() {
        return "2.0.21";
    }

}
