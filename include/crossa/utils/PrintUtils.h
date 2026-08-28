#pragma once

#include <string>
#include <string_view>

namespace crossa::utils {

// Centralizes plain and colored line output for Crossa utilities.
// Its println overloads keep console formatting in one replaceable location.
class PrintUtils final {
public:
    // Prints a message followed by a line terminator.
    static void println(const std::string& message);

    // Prints a colored message followed by a line terminator.
    static void println(
        const std::string& message,
        std::string_view color
    );

private:
    inline static constexpr std::string_view ResetColor = "\033[0m";
};

}
