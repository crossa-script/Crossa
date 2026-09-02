#include "crossa/cli/InteractiveCli.h"

#include <algorithm>
#include <stdexcept>
#include <system_error>
#include <utility>

#include "crossa/cli/CliValueValidation.h"
#include "crossa/cli/ProjectSourceDiscovery.h"
#include "crossa/cli/doctor/DoctorCommand.h"
#include "crossa/cli/doctor/DoctorChecks.h"
#include "crossa/cli/doctor/DoctorRunner.h"
#include "crossa/packaging/android/AndroidBuildRequirements.h"
#include "crossa/utils/PrintUtils.h"

using namespace std;

namespace crossa::cli {

namespace {

// Formats safe reusable shell commands and project-relative paths.
class InteractiveCommandFormatting final {
public:
    // Escapes one command argument for safe copy-and-paste in the host shell.
    [[nodiscard]] static string escapeShellArgument(const string& value) {
#if defined(_WIN32)
        string result = "\"";
        for (const char character : value) {
            if (character == '\\' || character == '\"') {
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

    // Returns a path relative to a normalized project root when possible.
    [[nodiscard]] static string relativePath(
        const filesystem::path& projectRoot,
        const filesystem::path& path
    ) {
        const filesystem::path relative = path.lexically_relative(projectRoot);
        return relative.empty()
            ? path.generic_string()
            : relative.generic_string();
    }
};

}

// Creates a wizard that can render the executable's reusable command form.
InteractiveCli::InteractiveCli(string executableArgument) :
    executableArgument_(std::move(executableArgument)),
    prompt_() {}

// Runs the wizard until it produces a normal command or exits.
InteractiveResult InteractiveCli::run() {
    utils::PrintUtils::println("");
    utils::PrintUtils::println("Crossa");
    utils::PrintUtils::println("Compiler-powered native SDK platform");

    Screen screen = Screen::Root;
    while (screen != Screen::Finished) {
        if (prompt_.wasCancelled()) {
            utils::PrintUtils::println("Crossa operation cancelled.");
            return InteractiveResult{
                InteractiveResultStatus::Cancelled,
                nullopt,
                ""
            };
        }
        if (screen == Screen::Root) {
            const InteractiveResult rootResult = runRoot(screen);
            if (rootResult.status != InteractiveResultStatus::Completed) {
                if (rootResult.status == InteractiveResultStatus::Cancelled) {
                    utils::PrintUtils::println("Crossa operation cancelled.");
                }
                return rootResult;
            }
            continue;
        }
        const InteractiveResult result = screen == Screen::Generate
            ? configureGeneration()
            : configureRun();
        if (result.status == InteractiveResultStatus::Completed) {
            return result;
        }
        if (result.status == InteractiveResultStatus::Exited) {
            screen = Screen::Root;
            continue;
        }
        if (result.status == InteractiveResultStatus::Cancelled) {
            utils::PrintUtils::println("Crossa operation cancelled.");
        }
        return result;
    }
    return InteractiveResult{
        InteractiveResultStatus::Exited,
        nullopt,
        ""
    };
}

// Prompts the root operation and advances the explicit screen state.
InteractiveResult InteractiveCli::runRoot(Screen& screen) {
    const optional<int> selection = prompt_.select(
        "What do you want to do?",
        {"Generate", "Run"},
        1
    );
    if (!selection.has_value()) {
        return InteractiveResult{
            InteractiveResultStatus::Cancelled,
            nullopt,
            ""
        };
    }
    if (selection.value() == 0) {
        utils::PrintUtils::println("Crossa operation cancelled.");
        screen = Screen::Finished;
        return InteractiveResult{
            InteractiveResultStatus::Exited,
            nullopt,
            ""
        };
    }
    screen = selection.value() == 1 ? Screen::Generate : Screen::Run;
    return InteractiveResult{
        InteractiveResultStatus::Completed,
        nullopt,
        ""
    };
}

// Collects Android generation settings for the existing build-project path.
InteractiveResult InteractiveCli::configureGeneration() {
    bool wentBack = false;
    const optional<filesystem::path> projectRoot = selectProjectRoot(wentBack);
    if (!projectRoot.has_value()) {
        return wentBack
            ? InteractiveResult{InteractiveResultStatus::Exited, nullopt, ""}
            : InteractiveResult{InteractiveResultStatus::Cancelled, nullopt, ""};
    }

    const optional<filesystem::path> entrySource = selectEntrySource(
        projectRoot.value(),
        wentBack
    );
    if (!entrySource.has_value()) {
        return wentBack
            ? InteractiveResult{InteractiveResultStatus::Exited, nullopt, ""}
            : InteractiveResult{InteractiveResultStatus::Cancelled, nullopt, ""};
    }

    const optional<int> platform = prompt_.select(
        "Which platform do you want to generate for?",
        {"Android", "iOS"},
        1
    );
    if (!platform.has_value()) {
        return InteractiveResult{InteractiveResultStatus::Cancelled, nullopt, ""};
    }
    if (platform.value() == 0) {
        return InteractiveResult{InteractiveResultStatus::Exited, nullopt, ""};
    }

    if (platform.value() == 2) {
        const string defaultOutput =
            (projectRoot.value() / "dist" / "ios").string();
        const optional<string> outputText = prompt_.text(
            "Output directory",
            defaultOutput
        );
        if (!outputText.has_value()) {
            return InteractiveResult{InteractiveResultStatus::Cancelled, nullopt, ""};
        }
        const filesystem::path outputDirectory = normalizePath(
            filesystem::path(outputText.value())
        );
        if (outputDirectory.empty()) {
            return failure("Output directory cannot be empty.");
        }
        const InteractiveCommand command{
            InteractiveCommandKind::GenerateIosFramework,
            projectRoot.value(),
            projectRoot,
            entrySource,
            outputDirectory,
            nullopt,
            nullopt,
            nullopt
        };
        if (!checkIosEnvironment()) {
            return failure(
                "iOS environment validation failed. Fix the reported issues "
                "before building the XCFramework."
            );
        }
        const string summary =
            "Review Crossa generation\n\n"
            "Platform       iOS\n"
            "Artifact       Debug and release XCFramework\n"
            "Project        " + projectRoot.value().string() + "\n"
            "Entry          " + InteractiveCommandFormatting::relativePath(
                projectRoot.value(), entrySource.value()
            ) + "\n"
            "Output         " + outputDirectory.string();
        const int decision = confirm(summary, renderCommand(command), "Build");
        return decision == 1
            ? InteractiveResult{InteractiveResultStatus::Completed, command, ""}
            : decision == 2
                ? InteractiveResult{InteractiveResultStatus::Exited, nullopt, ""}
                : InteractiveResult{InteractiveResultStatus::Cancelled, nullopt, ""};
    }

    const optional<int> outputKind = prompt_.select(
        "What should Crossa generate?",
        {"Buildable Android project (AAR assembly input)"},
        1
    );
    if (!outputKind.has_value()) {
        return InteractiveResult{InteractiveResultStatus::Cancelled, nullopt, ""};
    }
    if (outputKind.value() == 0) {
        return InteractiveResult{InteractiveResultStatus::Exited, nullopt, ""};
    }

    using AndroidBuildRequirements =
        packaging::android::AndroidBuildRequirements;
    bool versionWentBack = false;
    const optional<string> kotlinVersion = selectAndroidVersion(
        "Kotlin version",
        AndroidBuildRequirements::kotlinAndroidPluginVersion(),
        false,
        versionWentBack
    );
    if (!kotlinVersion.has_value()) {
        return versionWentBack
            ? InteractiveResult{InteractiveResultStatus::Exited, nullopt, ""}
            : InteractiveResult{InteractiveResultStatus::Cancelled, nullopt, ""};
    }
    const optional<string> ndkVersion = selectAndroidVersion(
        "Android NDK version",
        AndroidBuildRequirements::recommendedNdkVersion(),
        true,
        versionWentBack
    );
    if (!ndkVersion.has_value()) {
        return versionWentBack
            ? InteractiveResult{InteractiveResultStatus::Exited, nullopt, ""}
            : InteractiveResult{InteractiveResultStatus::Cancelled, nullopt, ""};
    }
    const optional<string> gradleVersion = selectAndroidVersion(
        "Gradle version",
        AndroidBuildRequirements::gradleWrapperVersion(),
        false,
        versionWentBack
    );
    if (!gradleVersion.has_value()) {
        return versionWentBack
            ? InteractiveResult{InteractiveResultStatus::Exited, nullopt, ""}
            : InteractiveResult{InteractiveResultStatus::Cancelled, nullopt, ""};
    }

    const string defaultOutput =
        (projectRoot.value() / "dist" / "android").string();
    const optional<string> outputText = prompt_.text(
        "Output directory",
        defaultOutput
    );
    if (!outputText.has_value()) {
        return InteractiveResult{InteractiveResultStatus::Cancelled, nullopt, ""};
    }
    const filesystem::path outputDirectory = normalizePath(
        filesystem::path(outputText.value())
    );
    if (outputDirectory.empty()) {
        return failure("Output directory cannot be empty.");
    }

    const InteractiveCommand command{
        InteractiveCommandKind::GenerateAndroidLibrary,
        projectRoot.value(),
        projectRoot,
        entrySource,
        outputDirectory,
        ndkVersion,
        gradleVersion,
        kotlinVersion
    };
    if (!checkAndroidEnvironment(ndkVersion)) {
        return failure(
            "Android environment validation failed. Fix the reported issues "
            "before generating the project."
        );
    }

    const string summary =
        "Review Crossa generation\n\n"
        "Platform       Android\n"
        "Artifact       Buildable Android project\n"
        "Project        " + projectRoot.value().string() + "\n"
        "Entry          " + InteractiveCommandFormatting::relativePath(
            projectRoot.value(),
            entrySource.value()
        ) + "\n"
        "Kotlin         " + kotlinVersion.value() + "\n"
        "NDK            " + ndkVersion.value() + "\n"
        "Gradle         " + gradleVersion.value() + "\n"
        "ABI            " + AndroidBuildRequirements::supportedAbi() + "\n"
        "Output         " + outputDirectory.string();
    const int decision = confirm(
        summary,
        renderCommand(command),
        "Generate"
    );
    if (decision == 1) {
        return InteractiveResult{
            InteractiveResultStatus::Completed,
            command,
            ""
        };
    }
    if (decision == 2) {
        return InteractiveResult{InteractiveResultStatus::Exited, nullopt, ""};
    }
    return InteractiveResult{InteractiveResultStatus::Cancelled, nullopt, ""};
}

// Collects a source and project root for native Crossa execution.
InteractiveResult InteractiveCli::configureRun() {
    bool wentBack = false;
    const optional<filesystem::path> projectRoot = selectProjectRoot(wentBack);
    if (!projectRoot.has_value()) {
        return wentBack
            ? InteractiveResult{InteractiveResultStatus::Exited, nullopt, ""}
            : InteractiveResult{InteractiveResultStatus::Cancelled, nullopt, ""};
    }
    const optional<filesystem::path> entrySource = selectEntrySource(
        projectRoot.value(),
        wentBack
    );
    if (!entrySource.has_value()) {
        return wentBack
            ? InteractiveResult{InteractiveResultStatus::Exited, nullopt, ""}
            : InteractiveResult{InteractiveResultStatus::Cancelled, nullopt, ""};
    }

    const InteractiveCommand command{
        InteractiveCommandKind::Run,
        entrySource.value(),
        projectRoot,
        nullopt,
        nullopt,
        nullopt,
        nullopt,
        nullopt
    };
    const string summary =
        "Review Crossa run\n\n"
        "Action         Run\n"
        "Project        " + projectRoot.value().string() + "\n"
        "Entry          " + InteractiveCommandFormatting::relativePath(
            projectRoot.value(),
            entrySource.value()
        );
    const int decision = confirm(summary, renderCommand(command), "Run");
    if (decision == 1) {
        return InteractiveResult{
            InteractiveResultStatus::Completed,
            command,
            ""
        };
    }
    if (decision == 2) {
        return InteractiveResult{InteractiveResultStatus::Exited, nullopt, ""};
    }
    return InteractiveResult{InteractiveResultStatus::Cancelled, nullopt, ""};
}

// Selects the current directory or a validated custom project directory.
optional<filesystem::path> InteractiveCli::selectProjectRoot(bool& wentBack) {
    wentBack = false;
    const filesystem::path currentDirectory = normalizePath(
        filesystem::current_path()
    );
    const optional<int> selection = prompt_.select(
        "Project root",
        {"Current directory (" + currentDirectory.string() + ")", "Custom path"},
        1
    );
    if (!selection.has_value()) {
        return nullopt;
    }
    if (selection.value() == 0) {
        wentBack = true;
        return nullopt;
    }
    if (selection.value() == 1) {
        return filesystem::is_directory(currentDirectory)
            ? optional<filesystem::path>(currentDirectory)
            : nullopt;
    }

    while (true) {
        const optional<string> value = prompt_.text("Enter project path", "");
        if (!value.has_value()) {
            return nullopt;
        }
        const filesystem::path path = normalizePath(filesystem::path(value.value()));
        if (!path.empty() && filesystem::is_directory(path)) {
            return path;
        }
        utils::PrintUtils::println(
            "Invalid project path. Enter an existing directory."
        );
    }
}

// Selects a deterministic source candidate or validates a custom source.
optional<filesystem::path> InteractiveCli::selectEntrySource(
    const filesystem::path& projectRoot,
    bool& wentBack
) {
    wentBack = false;
    vector<filesystem::path> candidates;
    try {
        candidates = ProjectSourceDiscovery::find(projectRoot);
    } catch (const exception& error) {
        utils::PrintUtils::println(error.what());
    }

    vector<string> options;
    options.reserve(candidates.size() + 1);
    for (const filesystem::path& candidate : candidates) {
        options.push_back(
            InteractiveCommandFormatting::relativePath(projectRoot, candidate)
        );
    }
    options.push_back("Custom path");
    const optional<int> selection = prompt_.select(
        "Select the Crossa source",
        options,
        1
    );
    if (!selection.has_value()) {
        return nullopt;
    }
    if (selection.value() == 0) {
        wentBack = true;
        return nullopt;
    }
    if (static_cast<size_t>(selection.value()) <= candidates.size()) {
        return candidates[selection.value() - 1];
    }

    while (true) {
        const optional<string> value = prompt_.text("Enter source path", "");
        if (!value.has_value()) {
            return nullopt;
        }
        const filesystem::path path = normalizePath(filesystem::path(value.value()));
        if (isValidEntry(projectRoot, path)) {
            return path;
        }
        utils::PrintUtils::println(
            "Invalid source path. Enter an existing .cra file inside the project."
        );
    }
}

// Collects one validated Android version override or its configured default.
optional<string> InteractiveCli::selectAndroidVersion(
    const string& title,
    const string& defaultValue,
    bool ndkVersion,
    bool& wentBack
) {
    wentBack = false;
    const optional<int> selection = prompt_.select(
        title,
        {"Crossa default (" + defaultValue + ")", "Custom version"},
        1
    );
    if (!selection.has_value()) {
        return nullopt;
    }
    if (selection.value() == 0) {
        wentBack = true;
        return nullopt;
    }
    if (selection.value() == 1) {
        return defaultValue;
    }

    while (true) {
        const optional<string> value = prompt_.text("Enter " + title, "");
        if (!value.has_value()) {
            return nullopt;
        }
        const bool valid = ndkVersion
            ? CliValueValidation::isNdkVersion(value.value())
            : CliValueValidation::isToolVersion(value.value());
        if (valid) {
            return value;
        }
        utils::PrintUtils::println(
            "Invalid version syntax. Enter a supported version value."
        );
    }
}

// Normalizes a user path without creating or modifying filesystem entries.
filesystem::path InteractiveCli::normalizePath(const filesystem::path& path) {
    error_code error;
    const filesystem::path normalized = filesystem::weakly_canonical(path, error);
    if (!error) {
        return normalized;
    }
    return filesystem::absolute(path, error);
}

// Returns true when a source path is a valid child of the project root.
bool InteractiveCli::isValidEntry(
    const filesystem::path& projectRoot,
    const filesystem::path& entrySource
) {
    error_code error;
    if (!filesystem::is_regular_file(entrySource, error) || error ||
        entrySource.extension() != ".cra") {
        return false;
    }
    const filesystem::path relative = entrySource.lexically_relative(projectRoot);
    return !relative.empty() && !relative.is_absolute() &&
        relative.begin()->string() != ".." &&
        entrySource.filename() != "config.cra";
}

// Returns a shell-escaped equivalent command for display and reuse.
string InteractiveCli::renderCommand(const InteractiveCommand& command) {
    string result = "crossa ";
    if (command.kind == InteractiveCommandKind::Run) {
        const bool usesCurrentProject = command.projectRoot.value() ==
            normalizePath(filesystem::current_path());
        result += "run " + InteractiveCommandFormatting::escapeShellArgument(
            usesCurrentProject
                ? InteractiveCommandFormatting::relativePath(
                    command.projectRoot.value(),
                    command.sourcePath
                )
                : command.sourcePath.string()
        );
        if (!usesCurrentProject) {
            result += " --project-root " +
                InteractiveCommandFormatting::escapeShellArgument(
                    command.projectRoot.value().string()
                );
        }
        return result;
    }

    result += command.kind == InteractiveCommandKind::GenerateIosFramework
        ? "generate-build ios "
        : "generate-build android ";
    result +=
        InteractiveCommandFormatting::escapeShellArgument(
            command.sourcePath.string()
        ) +
        " --entry " + InteractiveCommandFormatting::escapeShellArgument(
            InteractiveCommandFormatting::relativePath(
                command.sourcePath,
                command.entrySource.value()
            )
        ) +
        " --output " + InteractiveCommandFormatting::escapeShellArgument(
            command.outputDirectory.value().string()
        );
    if (command.kind == InteractiveCommandKind::GenerateIosFramework) {
        return result;
    }
    result +=
        " --ndk-version " + InteractiveCommandFormatting::escapeShellArgument(
            command.ndkVersion.value()
        ) +
        " --gradle-version " + InteractiveCommandFormatting::escapeShellArgument(
            command.gradleVersion.value()
        ) +
        " --kotlin-version " + InteractiveCommandFormatting::escapeShellArgument(
            command.kotlinVersion.value()
        );
    return result;
}

// Prints a review and returns the user's execution decision.
int InteractiveCli::confirm(
    const string& summary,
    const string& command,
    const string& action
) {
    utils::PrintUtils::println("");
    utils::PrintUtils::println(summary);
    utils::PrintUtils::println("");
    utils::PrintUtils::println("Equivalent command:");
    utils::PrintUtils::println(command);
    const optional<int> decision = prompt_.select(
        "Continue?",
        {action, "Change configuration"},
        1
    );
    return decision.value_or(0);
}

// Runs the shared Android doctor checks for the selected NDK.
bool InteractiveCli::checkAndroidEnvironment(
    const optional<string>& ndkVersion
) {
    utils::PrintUtils::println("");
    utils::PrintUtils::println("Checking Android environment...");
    const doctor::DoctorReport report = doctor::DoctorRunner().run(
        executableArgument_,
        ndkVersion
    );
    doctor::DoctorCommand::printReport(report);
    return !report.hasFailures();
}

// Runs only iOS-specific doctor rows so absent Android tools do not block iOS builds.
bool InteractiveCli::checkIosEnvironment() {
    utils::PrintUtils::println("");
    utils::PrintUtils::println("Checking iOS environment...");
    const vector<doctor::DoctorResult> results =
        doctor::AppleToolchainCheck().run();
    bool ready = true;
    for (const doctor::DoctorResult& result : results) {
        utils::PrintUtils::println(
            string(result.status == doctor::DoctorStatus::Passed ? "✓ " : "✗ ") +
            result.name + " " + result.value
        );
        ready = ready && result.status != doctor::DoctorStatus::Failed;
    }
    return ready;
}

// Prints a diagnostic and returns a failed interactive result.
InteractiveResult InteractiveCli::failure(string message) {
    utils::PrintUtils::println(message);
    return InteractiveResult{
        InteractiveResultStatus::Failed,
        nullopt,
        std::move(message)
    };
}

}
