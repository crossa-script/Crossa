#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "crossa/cli/doctor/DoctorResult.h"

namespace crossa::cli::doctor {

// Inspects the installed Crossa CLI and packaged assets.
// run() reports the executable, installation root, manifest, runtime assets, and templates.
class CrossaInstallationCheck final {
public:
    // Creates a check bound to the executable argument used to launch Crossa.
    explicit CrossaInstallationCheck(std::string executableArgument);

    // Returns structured Crossa installation results without writing output.
    [[nodiscard]] std::vector<DoctorResult> run() const;

private:
    std::string executableArgument_;
};

// Inspects the host operating system and CPU architecture.
// run() reports the platform details used by generated Android builds.
class HostCheck final {
public:
    // Returns structured host results without writing output.
    [[nodiscard]] std::vector<DoctorResult> run() const;
};

// Inspects the Android SDK root and required SDK packages.
// run() reports ANDROID_HOME, SDK availability, and sdkmanager status.
class AndroidSdkCheck final {
public:
    // Returns structured Android SDK results without writing output.
    [[nodiscard]] std::vector<DoctorResult> run() const;
};

// Inspects side-by-side Android NDK installations.
// run() validates that one installed NDK meets the generated build minimum.
class AndroidNdkCheck final {
public:
    // Returns structured Android NDK results without writing output.
    [[nodiscard]] std::vector<DoctorResult> run() const;

    // Returns structured results for the requested exact NDK version.
    [[nodiscard]] std::vector<DoctorResult> run(
        const std::string& requestedVersion
    ) const;
};

// Inspects the CMake executable available to Android native builds.
// run() validates that CMake executes and meets the generated build minimum.
class CMakeCheck final {
public:
    // Returns structured CMake results without writing output.
    [[nodiscard]] std::vector<DoctorResult> run() const;
};

// Inspects the Ninja executable available to Android native builds.
// run() validates that Ninja exists and can execute.
class NinjaCheck final {
public:
    // Returns structured Ninja results without writing output.
    [[nodiscard]] std::vector<DoctorResult> run() const;
};

// Inspects the Java executable available to generated Gradle builds.
// run() validates JAVA_HOME or PATH Java against the generated toolchain requirement.
class JavaCheck final {
public:
    // Returns structured Java results without writing output.
    [[nodiscard]] std::vector<DoctorResult> run() const;
};

// Inspects Apple tools required only when Crossa builds iOS XCFramework artifacts.
// run() reports Xcode, SDK, Swift, Clang, and CMake readiness without affecting other targets.
class AppleToolchainCheck final {
public:
    // Returns structured Apple iOS build-tool results without writing output.
    [[nodiscard]] std::vector<DoctorResult> run() const;
};

// Inspects writable locations used by Crossa build workflows.
// run() verifies the Crossa cache and temporary directory without leaving probe files.
class StorageCheck final {
public:
    // Returns structured storage results without writing output.
    [[nodiscard]] std::vector<DoctorResult> run() const;
};

}
