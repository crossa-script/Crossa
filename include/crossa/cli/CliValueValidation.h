#pragma once

#include <string_view>

namespace crossa::cli {

// Validates bounded CLI values before they reach compiler or build services.
class CliValueValidation final {
public:
    // Returns true for a numeric dotted Android NDK version.
    [[nodiscard]] static bool isNdkVersion(std::string_view version) noexcept;

    // Returns true for a safe Gradle or Kotlin tool version.
    [[nodiscard]] static bool isToolVersion(std::string_view version) noexcept;

    // Returns true for a three-part numeric semantic version.
    [[nodiscard]] static bool isSemanticVersion(std::string_view version) noexcept;

    // Returns true for an HTTPS package base URL without a trailing slash.
    [[nodiscard]] static bool isPackageBaseUrl(std::string_view url) noexcept;
};

}
