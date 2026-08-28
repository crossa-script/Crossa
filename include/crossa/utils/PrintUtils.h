#pragma once

#include <string>
#include <string_view>

namespace crossa::utils {

class PrintUtils final {
public:
    // Prints a string followed by a line terminator.
    static void println(const std::string& message);

    // Prints a colored string followed by a line terminator.
    static void println(
        const std::string& message,
        std::string_view color
    );

private:
    inline static constexpr std::string_view ResetColor = "\033[0m";
};

}
