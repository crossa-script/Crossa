#pragma once

#include <string>
#include <string_view>

namespace crossa::utils {

// Emits filtered Crossa diagnostics at error, warning, info, or debug level.
// info(), debug(), warning(), and error() provide level-aware output.
class Log final {
public:
    enum class Level {
        Error,
        Warning,
        Info,
        Debug
    };

    // Creates a logger configured with the requested minimum visibility.
    explicit Log(Level level = Level::Error) noexcept;

    // Prints an informational message when info logging is enabled.
    void info(const std::string& message) const;

    // Prints a detailed execution message in debug mode.
    void debug(const std::string& message) const;

    // Prints a warning message when warning logging is enabled.
    void warning(const std::string& message) const;

    // Prints an error message at every logging level.
    void error(const std::string& message) const;

private:
    // Returns whether a message should be emitted at the current level.
    [[nodiscard]] bool shouldLog(Level level) const noexcept;

    Level level_;
    inline static constexpr std::string_view WarningColor = "\033[33m";
    inline static constexpr std::string_view ErrorColor = "\033[31m";
};

}
