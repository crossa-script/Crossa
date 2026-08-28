#pragma once

#include <string>
#include <string_view>

namespace crossa::utils {

class Log final {
public:
    // Prints an informational log message.
    static void info(const std::string& message);

    // Prints a warning log message.
    static void warning(const std::string& message);

    // Prints an error log message.
    static void error(const std::string& message);

private:
    inline static constexpr std::string_view WarningColor = "\033[33m";
    inline static constexpr std::string_view ErrorColor = "\033[31m";
};

}
