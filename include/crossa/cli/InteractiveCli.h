#pragma once

#include <filesystem>
#include <optional>
#include <string>

#include "crossa/cli/ConsolePrompt.h"

namespace crossa::cli {

// Identifies the normal command produced by the interactive configuration flow.
enum class InteractiveCommandKind {
    Run,
    GenerateAndroidLibrary,
    GenerateIosFramework
};

// Stores typed values collected by the wizard before normal CLI dispatch.
struct InteractiveCommand final {
    InteractiveCommandKind kind;
    std::filesystem::path sourcePath;
    std::optional<std::filesystem::path> projectRoot;
    std::optional<std::filesystem::path> entrySource;
    std::optional<std::filesystem::path> outputDirectory;
    std::optional<std::string> ndkVersion;
    std::optional<std::string> gradleVersion;
    std::optional<std::string> kotlinVersion;
};

// Describes whether an interactive session produced work, exited, or failed.
enum class InteractiveResultStatus {
    Completed,
    Exited,
    Cancelled,
    Failed
};

// Owns one interactive CLI session and converts answers into normal commands.
struct InteractiveResult final {
    InteractiveResultStatus status;
    std::optional<InteractiveCommand> command;
    std::string error;
};

// Coordinates the no-argument terminal workflow without executing commands itself.
class InteractiveCli final {
public:
    // Creates a wizard that can render the executable's reusable command form.
    explicit InteractiveCli(std::string executableArgument);

    // Runs the wizard until it produces a normal command or exits.
    [[nodiscard]] InteractiveResult run();

private:
    enum class Screen {
        Root,
        Generate,
        Run,
        Finished
    };

    // Prompts the root operation and advances the explicit screen state.
    [[nodiscard]] InteractiveResult runRoot(Screen& screen);

    // Collects Android generation settings for the existing build-project path.
    [[nodiscard]] InteractiveResult configureGeneration();

    // Collects a source and project root for native Crossa execution.
    [[nodiscard]] InteractiveResult configureRun();

    // Selects the current directory or a validated custom project directory.
    [[nodiscard]] std::optional<std::filesystem::path> selectProjectRoot(
        bool& wentBack
    );

    // Selects a deterministic source candidate or validates a custom source.
    [[nodiscard]] std::optional<std::filesystem::path> selectEntrySource(
        const std::filesystem::path& projectRoot,
        bool& wentBack
    );

    // Collects one validated Android version override or its configured default.
    [[nodiscard]] std::optional<std::string> selectAndroidVersion(
        const std::string& title,
        const std::string& defaultValue,
        bool ndkVersion,
        bool& wentBack
    );

    // Normalizes a user path without creating or modifying filesystem entries.
    [[nodiscard]] static std::filesystem::path normalizePath(
        const std::filesystem::path& path
    );

    // Returns true when a source path is a valid child of the project root.
    [[nodiscard]] static bool isValidEntry(
        const std::filesystem::path& projectRoot,
        const std::filesystem::path& entrySource
    );

    // Returns a shell-escaped equivalent command for display and reuse.
    [[nodiscard]] static std::string renderCommand(
        const InteractiveCommand& command
    );

    // Prints a review and returns the user's execution decision.
    [[nodiscard]] int confirm(
        const std::string& summary,
        const std::string& command,
        const std::string& action
    );

    // Runs the shared Android doctor checks for the selected NDK.
    [[nodiscard]] bool checkAndroidEnvironment(
        const std::optional<std::string>& ndkVersion
    );

    // Runs only Apple iOS doctor checks before an XCFramework build is selected.
    [[nodiscard]] bool checkIosEnvironment();

    // Prints a diagnostic and returns a failed interactive result.
    [[nodiscard]] static InteractiveResult failure(std::string message);

    std::string executableArgument_;
    ConsolePrompt prompt_;
};

}
