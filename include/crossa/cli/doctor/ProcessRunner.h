#pragma once

#include <string>
#include <vector>

namespace crossa::cli::doctor {

// Stores captured process output for external tool version checks.
// started distinguishes execution failures from non-zero exit statuses.
struct ProcessResult final {
    bool started;
    int exitCode;
    std::string output;
};

// Executes fixed local tool commands and captures their text output.
// run() quotes command arguments before invoking the host shell.
class ProcessRunner final {
public:
    // Runs one executable with fixed arguments and captures stdout and stderr.
    [[nodiscard]] static ProcessResult run(
        const std::vector<std::string>& command
    );

private:
    // Quotes one command segment for safe shell execution.
    [[nodiscard]] static std::string quote(const std::string& value);
};

}
