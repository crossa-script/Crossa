#include "crossa/utils/Log.h"

#include "crossa/utils/PrintUtils.h"

using namespace std;

namespace crossa::utils {

    // Creates a logger configured with the requested minimum visibility.
    Log::Log(Level level) noexcept : level_(level) {}

    // Prints an informational message when info logging is enabled.
    void Log::info(const string& message) const {
        if (shouldLog(Level::Info)) {
            PrintUtils::println("[INFO] " + message);
        }
    }

    // Prints a detailed execution message in debug mode.
    void Log::debug(const string& message) const {
        if (shouldLog(Level::Debug)) {
            PrintUtils::println("[DEBUG] " + message);
        }
    }

    // Prints a warning message when warning logging is enabled.
    void Log::warning(const string& message) const {
        if (shouldLog(Level::Warning)) {
            PrintUtils::println("[WARNING] " + message, WarningColor);
        }
    }

    // Prints an error message at every logging level.
    void Log::error(const string& message) const {
        PrintUtils::println("[ERROR] " + message, ErrorColor);
    }

    // Returns whether a message should be emitted at the current level.
    bool Log::shouldLog(Level level) const noexcept {
        return static_cast<int>(level) <= static_cast<int>(level_);
    }

}
