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

}
