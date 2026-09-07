#pragma once

#include <filesystem>
#include <optional>
#include <string>

namespace crossa::packaging::android {

// Defines Android build requirements shared by generation and environment checks.
// compileSdkVersion(), ndkVersion(), recommendedNdkVersion(), cmakeMinimumVersion(), and javaToolchainVersion() expose the Android build contract.
class AndroidBuildRequirements final {
public:
    // Returns the Android SDK platform API required by generated AAR builds.
    [[nodiscard]] static int compileSdkVersion() noexcept;

    // Returns the minimum side-by-side Android NDK version required by generated AAR builds.
    [[nodiscard]] static std::string ndkVersion();

    // Returns the installed Android NDK version selected for generated Gradle builds.
    [[nodiscard]] static std::string recommendedNdkVersion();

    // Returns the minimum CMake version declared by generated native build files.
    [[nodiscard]] static std::string cmakeMinimumVersion();

    // Returns the Java toolchain major version required by generated Gradle builds.
    [[nodiscard]] static int javaToolchainVersion() noexcept;

    // Returns the Android Gradle Plugin version used by generated AAR builds.
    [[nodiscard]] static std::string androidGradlePluginVersion();

    // Returns the Kotlin Android plugin version used by generated AAR builds.
    [[nodiscard]] static std::string kotlinAndroidPluginVersion();

    // Returns the Gradle version supplied by the trusted generated wrapper.
    [[nodiscard]] static std::string gradleWrapperVersion();

    // Returns the single Android ABI currently packaged by generated AARs.
    [[nodiscard]] static std::string supportedAbi();

    [[nodiscard]] static std::optional<std::filesystem::path> androidSdkDirectory();

    // Returns the pinned OpenSSL source version used by generated Android projects.
    [[nodiscard]] static std::string openSslVersion();

    // Returns the verified official OpenSSL source archive URL.
    [[nodiscard]] static std::string openSslArchiveUrl();

    // Returns the SHA-256 digest for the pinned OpenSSL archive.
    [[nodiscard]] static std::string openSslArchiveSha256();

    // Returns the pinned libcurl source version used by generated Android projects.
    [[nodiscard]] static std::string curlVersion();

    // Returns the verified official libcurl source archive URL.
    [[nodiscard]] static std::string curlArchiveUrl();

    // Returns the SHA-256 digest for the pinned libcurl archive.
    [[nodiscard]] static std::string curlArchiveSha256();

    // Returns the pinned Mozilla CA bundle release used by generated Android projects.
    [[nodiscard]] static std::string caBundleVersion();

    // Returns the verified CA bundle URL maintained by the curl project.
    [[nodiscard]] static std::string caBundleUrl();

    // Returns the SHA-256 digest for the pinned CA bundle.
    [[nodiscard]] static std::string caBundleSha256();
};

}
