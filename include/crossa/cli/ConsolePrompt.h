#pragma once

#include <csignal>
#include <cstddef>
#include <optional>
#include <string>
#include <vector>

namespace crossa::cli {

// Reads bounded, cancellable terminal input and renders numbered prompts.
class ConsolePrompt final {
public:
    // Installs the session-local Ctrl+C handler used by interactive prompts.
    ConsolePrompt();

    // Restores the process Ctrl+C handler after the prompt session ends.
    ~ConsolePrompt();

    ConsolePrompt(const ConsolePrompt&) = delete;
    ConsolePrompt& operator=(const ConsolePrompt&) = delete;

    // Reads a numbered selection, applying the default for an empty line.
    [[nodiscard]] std::optional<int> select(
        const std::string& title,
        const std::vector<std::string>& options,
        int defaultOption
    );

    // Reads bounded text, applying the supplied default for an empty line.
    [[nodiscard]] std::optional<std::string> text(
        const std::string& title,
        const std::string& defaultValue
    );

    // Returns true after Ctrl+C was received by the prompt session.
    [[nodiscard]] bool wasCancelled() const noexcept;

private:
    using SignalHandler = void (*)(int);

    // Records Ctrl+C without performing non-signal-safe console operations.
    static void handleInterrupt(int signal) noexcept;

    // Reads one bounded line and reports EOF, cancellation, or overlong input.
    [[nodiscard]] std::optional<std::string> readLine(bool& tooLong);

    SignalHandler previousSignalHandler_;
    bool signalHandlerInstalled_;
    inline static volatile std::sig_atomic_t InterruptRequested = 0;
    inline static constexpr std::size_t MaxInputLength = 4096;
};

}
