#pragma once

namespace crossa::cli {

// Detects whether the current process has an interactive terminal available.
class TerminalCapabilities final {
public:
    // Returns true when standard input is attached to a terminal.
    [[nodiscard]] static bool isInteractiveInput() noexcept;

    // Returns true when standard output is attached to a terminal.
    [[nodiscard]] static bool isInteractiveOutput() noexcept;
};

}
