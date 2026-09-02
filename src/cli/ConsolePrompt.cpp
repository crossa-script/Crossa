#include "crossa/cli/ConsolePrompt.h"

#include <algorithm>
#include <cerrno>
#include <iostream>
#include <limits>

#if !defined(_WIN32)
#include <sys/select.h>
#include <unistd.h>
#endif

#include "crossa/utils/PrintUtils.h"

using namespace std;

namespace crossa::cli {

// Installs the session-local Ctrl+C handler used by interactive prompts.
ConsolePrompt::ConsolePrompt() :
    previousSignalHandler_(SIG_DFL),
    signalHandlerInstalled_(false) {
    InterruptRequested = 0;
    previousSignalHandler_ = signal(SIGINT, &ConsolePrompt::handleInterrupt);
    signalHandlerInstalled_ = previousSignalHandler_ != SIG_ERR;
}

// Restores the process Ctrl+C handler after the prompt session ends.
ConsolePrompt::~ConsolePrompt() {
    if (signalHandlerInstalled_) {
        signal(SIGINT, previousSignalHandler_);
    }
}

// Reads a numbered selection, applying the default for an empty line.
optional<int> ConsolePrompt::select(
    const string& title,
    const vector<string>& options,
    int defaultOption
) {
    while (!wasCancelled()) {
        utils::PrintUtils::println("");
        utils::PrintUtils::println(title);
        for (size_t index = 0; index < options.size(); ++index) {
            utils::PrintUtils::println(
                "  " + to_string(index + 1) + ". " + options[index]
            );
        }
        utils::PrintUtils::println("  0. " + string(
            title == "What do you want to do?" ? "Exit" : "Back"
        ));
        utils::PrintUtils::println(
            "Select [" + to_string(defaultOption) + "]:"
        );
        utils::PrintUtils::println(">");

        bool tooLong = false;
        const optional<string> line = readLine(tooLong);
        if (!line.has_value()) {
            return nullopt;
        }
        if (tooLong) {
            utils::PrintUtils::println(
                "Input is too long. Enter a numbered selection."
            );
            continue;
        }
        if (line->empty()) {
            return defaultOption;
        }

        const bool numeric = all_of(
            line->begin(),
            line->end(),
            [](const char character) {
                return character >= '0' && character <= '9';
            }
        );
        size_t parsed = numeric_limits<size_t>::max();
        if (numeric) {
            try {
                parsed = stoul(*line);
            } catch (...) {
                parsed = numeric_limits<size_t>::max();
            }
        }
        if (parsed <= options.size()) {
            return static_cast<int>(parsed);
        }
        utils::PrintUtils::println(
            "Invalid selection. Enter a value between 0 and " +
            to_string(options.size()) + "."
        );
    }
    return nullopt;
}

// Reads bounded text, applying the supplied default for an empty line.
optional<string> ConsolePrompt::text(
    const string& title,
    const string& defaultValue
) {
    while (!wasCancelled()) {
        utils::PrintUtils::println("");
        utils::PrintUtils::println(
            defaultValue.empty() ? title : title + " [" + defaultValue + "]"
        );
        utils::PrintUtils::println(">");
        bool tooLong = false;
        const optional<string> line = readLine(tooLong);
        if (!line.has_value()) {
            return nullopt;
        }
        if (tooLong) {
            utils::PrintUtils::println(
                "Input is too long. Enter a shorter value."
            );
            continue;
        }
        return line->empty() ? defaultValue : line;
    }
    return nullopt;
}

// Returns true after Ctrl+C was received by the prompt session.
bool ConsolePrompt::wasCancelled() const noexcept {
    return InterruptRequested != 0;
}

// Records Ctrl+C without performing non-signal-safe console operations.
void ConsolePrompt::handleInterrupt(int signal) noexcept {
    (void)signal;
    InterruptRequested = 1;
}

// Reads one bounded line and reports EOF, cancellation, or overlong input.
optional<string> ConsolePrompt::readLine(bool& tooLong) {
    tooLong = false;
    string line;
    line.reserve(MaxInputLength);
    while (true) {
        if (wasCancelled()) {
            return nullopt;
        }
#if defined(_WIN32)
        const int value = cin.get();
        if (value == EOF) {
            if (wasCancelled() || cin.eof()) {
                return nullopt;
            }
            cin.clear();
            return nullopt;
        }
#else
        fd_set inputSet;
        FD_ZERO(&inputSet);
        FD_SET(STDIN_FILENO, &inputSet);
        timeval timeout{0, 100000};
        const int ready = ::select(
            STDIN_FILENO + 1,
            &inputSet,
            nullptr,
            nullptr,
            &timeout
        );
        if (ready < 0) {
            if (errno == EINTR) {
                continue;
            }
            return nullopt;
        }
        if (ready == 0) {
            continue;
        }
        char character = '\0';
        const ssize_t bytesRead = read(STDIN_FILENO, &character, 1);
        if (bytesRead <= 0) {
            return nullopt;
        }
        const int value = static_cast<unsigned char>(character);
#endif
        if (value == '\n') {
            return line;
        }
        if (value == '\r') {
            continue;
        }
        if (line.size() == MaxInputLength) {
            tooLong = true;
            continue;
        }
        line.push_back(static_cast<char>(value));
    }
}

}
