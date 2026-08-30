#include "crossa/cli/doctor/ProcessRunner.h"

#include <array>
#include <cstdio>

using namespace std;

namespace crossa::cli::doctor {

    // Runs one executable with fixed arguments and captures stdout and stderr.
    ProcessResult ProcessRunner::run(const vector<string>& command) {
        if (command.empty()) {
            return ProcessResult{false, 1, ""};
        }

        string shellCommand;
        for (size_t index = 0; index < command.size(); ++index) {
            if (index > 0) {
                shellCommand += " ";
            }
            shellCommand += quote(command[index]);
        }
        shellCommand += " 2>&1";

        FILE* pipe =
#if defined(_WIN32)
            _popen(shellCommand.c_str(), "r");
#else
            popen(shellCommand.c_str(), "r");
#endif
        if (pipe == nullptr) {
            return ProcessResult{false, 1, ""};
        }

        string output;
        array<char, 256> buffer{};
        while (fgets(buffer.data(), static_cast<int>(buffer.size()), pipe) !=
               nullptr) {
            output += buffer.data();
        }
        const int exitCode =
#if defined(_WIN32)
            _pclose(pipe);
#else
            pclose(pipe);
#endif
        return ProcessResult{true, exitCode, output};
    }

    // Quotes one command segment for safe shell execution.
    string ProcessRunner::quote(const string& value) {
#if defined(_WIN32)
        string result = "\"";
        for (const char character : value) {
            if (character == '"' || character == '\\') {
                result += '\\';
            }
            result += character;
        }
        result += "\"";
        return result;
#else
        string result = "'";
        for (const char character : value) {
            if (character == '\'') {
                result += "'\\''";
            } else {
                result += character;
            }
        }
        result += "'";
        return result;
#endif
    }

}
