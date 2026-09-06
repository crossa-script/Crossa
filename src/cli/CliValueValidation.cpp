#include "crossa/cli/CliValueValidation.h"

namespace crossa::cli {

// Returns true for a numeric dotted Android NDK version.
bool CliValueValidation::isNdkVersion(std::string_view version) noexcept {
    if (version.empty() || version.front() == '.' || version.back() == '.') {
        return false;
    }
    bool previousWasSeparator = false;
    for (const char character : version) {
        if (character == '.') {
            if (previousWasSeparator) {
                return false;
            }
            previousWasSeparator = true;
            continue;
        }
        if (character < '0' || character > '9') {
            return false;
        }
        previousWasSeparator = false;
    }
    return true;
}

// Returns true for a safe Gradle or Kotlin tool version.
bool CliValueValidation::isToolVersion(std::string_view version) noexcept {
    if (version.empty()) {
        return false;
    }
    for (const char character : version) {
        const bool isLetter = (character >= 'A' && character <= 'Z') ||
            (character >= 'a' && character <= 'z');
        const bool isDigit = character >= '0' && character <= '9';
        if (!isLetter && !isDigit && character != '.' && character != '-' &&
            character != '_' && character != '+') {
            return false;
        }
    }
    return true;
}

// Returns true for a three-part numeric semantic version.
bool CliValueValidation::isSemanticVersion(std::string_view version) noexcept {
    if (!isNdkVersion(version)) {
        return false;
    }
    size_t separators = 0;
    for (const char character : version) {
        if (character == '.') {
            separators += 1;
        }
    }
    return separators == 2;
}

// Returns true for an HTTPS package base URL without a trailing slash.
bool CliValueValidation::isPackageBaseUrl(std::string_view url) noexcept {
    constexpr std::string_view Prefix = "https://";
    if (url.size() <= Prefix.size() || url.back() == '/' ||
        url.substr(0, Prefix.size()) != Prefix) {
        return false;
    }
    for (const char character : url) {
        if (character <= 32 || character == ' ') {
            return false;
        }
    }
    return true;
}

}
