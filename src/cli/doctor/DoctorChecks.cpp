#include "crossa/cli/doctor/DoctorChecks.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <fstream>
#include <optional>
#include <sstream>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

#include <unistd.h>

#include "crossa/cli/CrossaVersion.h"
#include "crossa/cli/doctor/ProcessRunner.h"
#include "crossa/packaging/android/AndroidBuildRequirements.h"

using namespace std;

namespace crossa::cli::doctor {

    // Provides focused filesystem, environment, and version helpers for checks.
    // Static helpers keep doctor checks deterministic without storing mutable state.
    class DoctorCheckUtils final {
    public:
        // Returns the current Crossa CLI version.
        [[nodiscard]] static string crossaVersion() {
            return CrossaVersion::current();
        }

        // Returns an environment variable value when it is configured.
        [[nodiscard]] static optional<string> environmentValue(
            const string& name
        ) {
            const char* value = getenv(name.c_str());
            if (value == nullptr || string(value).empty()) {
                return nullopt;
            }
            return string(value);
        }

        // Resolves an executable argument to an absolute filesystem path.
        [[nodiscard]] static filesystem::path executablePath(
            const string& executableArgument
        ) {
            const filesystem::path argumentPath(executableArgument);
            if (argumentPath.has_parent_path() || argumentPath.is_absolute()) {
                return canonicalOrAbsolute(argumentPath);
            }
            const optional<filesystem::path> pathExecutable =
                findExecutable(executableArgument);
            if (pathExecutable.has_value()) {
                return canonicalOrAbsolute(pathExecutable.value());
            }
            return canonicalOrAbsolute(argumentPath);
        }

        // Resolves the Crossa installation root from the executable location.
        [[nodiscard]] static filesystem::path installationRoot(
            const filesystem::path& executablePath
        ) {
            const filesystem::path executableDirectory =
                executablePath.parent_path();
            if (executableDirectory.filename() == "bin") {
                const filesystem::path distributionRoot =
                    executableDirectory.parent_path();
                const filesystem::path versionRoot = distributionRoot /
                    "versions" / crossaVersion();
                if (filesystem::is_directory(versionRoot)) {
                    return versionRoot;
                }
                return distributionRoot;
            }
            return executableDirectory;
        }

        // Returns true when a file exists and can be opened for reading.
        [[nodiscard]] static bool isReadableFile(const filesystem::path& path) {
            if (!filesystem::is_regular_file(path)) {
                return false;
            }
            ifstream input(path, ios::binary);
            return input.is_open();
        }

        // Returns true when a directory exists and can be inspected.
        [[nodiscard]] static bool isReadableDirectory(
            const filesystem::path& path
        ) {
            error_code error;
            if (!filesystem::is_directory(path, error) || error) {
                return false;
            }
            filesystem::directory_iterator iterator(path, error);
            return !error;
        }

        // Returns true when a path exists and has an executable permission bit.
        [[nodiscard]] static bool isExecutable(const filesystem::path& path) {
            error_code error;
            if (!filesystem::is_regular_file(path, error) || error) {
                return false;
            }
            const filesystem::perms permissions =
                filesystem::status(path, error).permissions();
            if (error) {
                return false;
            }
            using filesystem::perms;
            return (permissions & perms::owner_exec) != perms::none ||
                (permissions & perms::group_exec) != perms::none ||
                (permissions & perms::others_exec) != perms::none;
        }

        // Finds one executable on PATH.
        [[nodiscard]] static optional<filesystem::path> findExecutable(
            const string& executableName
        ) {
            const optional<string> pathValue = environmentValue("PATH");
            if (!pathValue.has_value()) {
                return nullopt;
            }
            for (const string& directory : splitPathList(pathValue.value())) {
                const filesystem::path candidate =
                    filesystem::path(directory) / executableName;
                if (isExecutable(candidate)) {
                    return candidate;
                }
            }
            return nullopt;
        }

        // Returns the configured Android SDK directory from ANDROID_HOME.
        [[nodiscard]] static optional<filesystem::path> androidHome() {
            const optional<string> value = environmentValue("ANDROID_HOME");
            if (!value.has_value()) {
                return nullopt;
            }
            return filesystem::path(value.value());
        }

        // Returns the best sdkmanager candidate inside one Android SDK.
        [[nodiscard]] static optional<filesystem::path> sdkManagerPath(
            const filesystem::path& sdkDirectory
        ) {
            vector<filesystem::path> candidates;
            candidates.push_back(
                sdkDirectory / "cmdline-tools" / "latest" / "bin" /
                "sdkmanager"
            );

            const filesystem::path commandLineTools =
                sdkDirectory / "cmdline-tools";
            error_code error;
            if (filesystem::is_directory(commandLineTools, error) && !error) {
                for (const filesystem::directory_entry& entry :
                     filesystem::directory_iterator(commandLineTools, error)) {
                    if (error) {
                        break;
                    }
                    if (entry.is_directory(error)) {
                        candidates.push_back(
                            entry.path() / "bin" / "sdkmanager"
                        );
                    }
                }
            }
            candidates.push_back(
                sdkDirectory / "tools" / "bin" / "sdkmanager"
            );
            sort(candidates.begin(), candidates.end());
            for (const filesystem::path& candidate : candidates) {
                if (isExecutable(candidate)) {
                    return candidate;
                }
            }
            return nullopt;
        }

        // Returns every SDK-managed CMake executable candidate.
        [[nodiscard]] static vector<filesystem::path> sdkCMakeExecutables(
            const filesystem::path& sdkDirectory
        ) {
            vector<filesystem::path> candidates;
            const filesystem::path cmakeDirectory = sdkDirectory / "cmake";
            error_code error;
            if (!filesystem::is_directory(cmakeDirectory, error) || error) {
                return candidates;
            }

            for (const filesystem::directory_entry& entry :
                 filesystem::directory_iterator(cmakeDirectory, error)) {
                if (error) {
                    break;
                }
                const filesystem::path candidate =
                    entry.path() / "bin" / "cmake";
                if (isExecutable(candidate)) {
                    candidates.push_back(candidate);
                }
            }
            sort(
                candidates.begin(),
                candidates.end(),
                [](const filesystem::path& left,
                   const filesystem::path& right) {
                    return compareVersions(
                        left.parent_path().parent_path().filename().string(),
                        right.parent_path().parent_path().filename().string()
                    ) > 0;
                }
            );
            return candidates;
        }

        // Returns every SDK-managed Ninja executable candidate.
        [[nodiscard]] static vector<filesystem::path> sdkNinjaExecutables(
            const filesystem::path& sdkDirectory
        ) {
            vector<filesystem::path> candidates;
            const filesystem::path cmakeDirectory = sdkDirectory / "cmake";
            error_code error;
            if (!filesystem::is_directory(cmakeDirectory, error) || error) {
                return candidates;
            }

            for (const filesystem::directory_entry& entry :
                 filesystem::directory_iterator(cmakeDirectory, error)) {
                if (error) {
                    break;
                }
                const filesystem::path candidate =
                    entry.path() / "bin" / "ninja";
                if (isExecutable(candidate)) {
                    candidates.push_back(candidate);
                }
            }
            sort(candidates.begin(), candidates.end());
            return candidates;
        }

        // Returns true when one SDK directory has required platform packages.
        [[nodiscard]] static bool hasRequiredSdkStructure(
            const filesystem::path& sdkDirectory,
            string& missing
        ) {
            const int compileSdk =
                packaging::android::AndroidBuildRequirements::
                    compileSdkVersion();
            const filesystem::path platformDirectory = sdkDirectory /
                "platforms" / ("android-" + to_string(compileSdk));
            if (!filesystem::is_directory(platformDirectory)) {
                missing = "missing platforms/android-" +
                    to_string(compileSdk);
                return false;
            }
            const filesystem::path buildToolsDirectory =
                sdkDirectory / "build-tools";
            if (!hasChildDirectory(buildToolsDirectory)) {
                missing = "missing build-tools";
                return false;
            }
            return true;
        }

        // Returns the Crossa cache directory used by local build workflows.
        [[nodiscard]] static optional<filesystem::path> cacheDirectory() {
            const optional<string> explicitCache =
                environmentValue("CROSSA_CACHE_DIR");
            if (explicitCache.has_value()) {
                return filesystem::path(explicitCache.value());
            }
            const optional<string> home = environmentValue("HOME");
            if (!home.has_value()) {
                return nullopt;
            }
            return filesystem::path(home.value()) / ".crossa" / "cache";
        }

        // Verifies a path can be written or created without leaving a probe file.
        [[nodiscard]] static bool canUseWritablePath(
            const filesystem::path& path
        ) {
            const optional<filesystem::path> probeDirectory =
                existingProbeDirectory(path);
            if (!probeDirectory.has_value()) {
                return false;
            }

            const filesystem::path probePath = probeDirectory.value() /
                (".crossa-doctor-" + to_string(getpid()) + ".tmp");
            ofstream output(probePath, ios::binary | ios::trunc);
            if (!output.is_open()) {
                return false;
            }
            output << "doctor";
            output.close();
            if (!output) {
                error_code removeError;
                filesystem::remove(probePath, removeError);
                return false;
            }
            error_code removeError;
            filesystem::remove(probePath, removeError);
            return !removeError;
        }

        // Returns the first line of text with surrounding whitespace removed.
        [[nodiscard]] static string firstTrimmedLine(const string& value) {
            const size_t lineEnd = value.find('\n');
            return trim(
                value.substr(
                    0,
                    lineEnd == string::npos ? string::npos : lineEnd
                )
            );
        }

        // Extracts a CMake semantic version from cmake --version output.
        [[nodiscard]] static string parseCMakeVersion(const string& output) {
            const string prefix = "cmake version ";
            const size_t start = output.find(prefix);
            if (start == string::npos) {
                return "";
            }
            return trim(readVersionToken(output, start + prefix.size()));
        }

        // Extracts a Java version from java -version output.
        [[nodiscard]] static string parseJavaVersion(const string& output) {
            const size_t quoteStart = output.find('"');
            if (quoteStart == string::npos) {
                return "";
            }
            const size_t quoteEnd = output.find('"', quoteStart + 1);
            if (quoteEnd == string::npos) {
                return "";
            }
            return output.substr(quoteStart + 1, quoteEnd - quoteStart - 1);
        }

        // Returns the Java major version from a parsed Java version string.
        [[nodiscard]] static int javaMajorVersion(const string& version) {
            const vector<int> parts = versionParts(version);
            if (parts.empty()) {
                return 0;
            }
            if (parts[0] == 1 && parts.size() > 1) {
                return parts[1];
            }
            return parts[0];
        }

        // Compares dotted numeric version strings.
        [[nodiscard]] static int compareVersions(
            const string& left,
            const string& right
        ) {
            vector<int> leftParts = versionParts(left);
            vector<int> rightParts = versionParts(right);
            const size_t partCount = max(leftParts.size(), rightParts.size());
            leftParts.resize(partCount);
            rightParts.resize(partCount);
            for (size_t index = 0; index < partCount; ++index) {
                if (leftParts[index] < rightParts[index]) {
                    return -1;
                }
                if (leftParts[index] > rightParts[index]) {
                    return 1;
                }
            }
            return 0;
        }

        // Returns the current host operating system label.
        [[nodiscard]] static string operatingSystemName() {
#if defined(__APPLE__)
            return "macOS";
#elif defined(__linux__)
            return "Linux";
#elif defined(_WIN32)
            return "Windows";
#else
            return "unknown";
#endif
        }

        // Returns the current host CPU architecture label.
        [[nodiscard]] static string architectureName() {
#if defined(__aarch64__) || defined(__arm64__)
            return "arm64";
#elif defined(__x86_64__) || defined(_M_X64)
            return "x86_64";
#elif defined(__arm__)
            return "arm";
#elif defined(__i386__) || defined(_M_IX86)
            return "x86";
#else
            return "unknown";
#endif
        }

    private:
        // Resolves a path canonically when possible and absolutely otherwise.
        [[nodiscard]] static filesystem::path canonicalOrAbsolute(
            const filesystem::path& path
        ) {
            error_code error;
            const filesystem::path canonicalPath =
                filesystem::weakly_canonical(path, error);
            if (!error) {
                return canonicalPath;
            }
            return filesystem::absolute(path);
        }

        // Splits a PATH-like environment variable into directories.
        [[nodiscard]] static vector<string> splitPathList(const string& value) {
            vector<string> directories;
            string current;
            stringstream stream(value);
            while (getline(stream, current, ':')) {
                if (!current.empty()) {
                    directories.push_back(current);
                }
            }
            return directories;
        }

        // Returns true when a directory contains at least one child directory.
        [[nodiscard]] static bool hasChildDirectory(
            const filesystem::path& directory
        ) {
            error_code error;
            if (!filesystem::is_directory(directory, error) || error) {
                return false;
            }
            for (const filesystem::directory_entry& entry :
                 filesystem::directory_iterator(directory, error)) {
                if (error) {
                    return false;
                }
                if (entry.is_directory(error) && !error) {
                    return true;
                }
            }
            return false;
        }

        // Finds an existing directory that can safely receive a probe file.
        [[nodiscard]] static optional<filesystem::path> existingProbeDirectory(
            const filesystem::path& path
        ) {
            error_code error;
            if (filesystem::is_directory(path, error) && !error) {
                return path;
            }

            filesystem::path current = path.parent_path();
            while (!current.empty()) {
                if (filesystem::is_directory(current, error) && !error) {
                    return current;
                }
                current = current.parent_path();
            }
            return nullopt;
        }

        // Removes surrounding ASCII whitespace from text.
        [[nodiscard]] static string trim(const string& value) {
            size_t start = 0;
            while (start < value.size() &&
                   isspace(static_cast<unsigned char>(value[start]))) {
                ++start;
            }
            size_t end = value.size();
            while (end > start &&
                   isspace(static_cast<unsigned char>(value[end - 1]))) {
                --end;
            }
            return value.substr(start, end - start);
        }

        // Reads one version token starting at the requested byte offset.
        [[nodiscard]] static string readVersionToken(
            const string& value,
            size_t start
        ) {
            size_t end = start;
            while (end < value.size() &&
                   (isdigit(static_cast<unsigned char>(value[end])) ||
                    value[end] == '.')) {
                ++end;
            }
            return value.substr(start, end - start);
        }

        // Parses numeric version parts and ignores suffixes after each part.
        [[nodiscard]] static vector<int> versionParts(const string& version) {
            vector<int> parts;
            size_t index = 0;
            while (index < version.size()) {
                while (index < version.size() &&
                       !isdigit(static_cast<unsigned char>(version[index]))) {
                    ++index;
                }
                if (index >= version.size()) {
                    break;
                }
                int value = 0;
                while (index < version.size() &&
                       isdigit(static_cast<unsigned char>(version[index]))) {
                    value = value * 10 + version[index] - '0';
                    ++index;
                }
                parts.push_back(value);
                while (index < version.size() && version[index] != '.') {
                    ++index;
                }
                if (index < version.size() && version[index] == '.') {
                    ++index;
                }
            }
            return parts;
        }
    };

    // Creates a check bound to the executable argument used to launch Crossa.
    CrossaInstallationCheck::CrossaInstallationCheck(
        string executableArgument
    ) : executableArgument_(std::move(executableArgument)) {}

    // Returns structured Crossa installation results without writing output.
    vector<DoctorResult> CrossaInstallationCheck::run() const {
        vector<DoctorResult> results;
        const filesystem::path executablePath =
            DoctorCheckUtils::executablePath(executableArgument_);
        const filesystem::path installationRoot =
            DoctorCheckUtils::installationRoot(executablePath);
        const filesystem::path manifestPath = installationRoot /
            "manifest.json";
        const filesystem::path runtimePath = installationRoot / "runtime";
        const filesystem::path templatesPath = installationRoot / "templates";
        const bool hasManifest = filesystem::exists(manifestPath);
        const bool manifestReadable = DoctorCheckUtils::isReadableFile(
            manifestPath
        );
        const bool runtimeReadable = DoctorCheckUtils::isReadableDirectory(
            runtimePath
        );
        const bool templatesReadable = DoctorCheckUtils::isReadableDirectory(
            templatesPath
        );
        const bool expectsExternalAssets = hasManifest ||
            filesystem::exists(runtimePath) ||
            filesystem::exists(templatesPath);

        results.push_back(DoctorResult{
            DoctorStatus::Passed,
            "Crossa",
            "Crossa CLI",
            DoctorCheckUtils::crossaVersion(),
            ""
        });
        results.push_back(DoctorResult{
            DoctorCheckUtils::isReadableDirectory(installationRoot)
                ? DoctorStatus::Passed
                : DoctorStatus::Warning,
            "Crossa",
            "Installation",
            installationRoot.string(),
            "Reinstall Crossa or run the installed crossa binary from its distribution."
        });
        if (hasManifest) {
            results.push_back(DoctorResult{
                manifestReadable ? DoctorStatus::Passed : DoctorStatus::Failed,
                "Crossa",
                "Release manifest",
                manifestReadable ? "available" : "not readable",
                "Reinstall Crossa so manifest.json is readable."
            });
        }
        results.push_back(DoctorResult{
            !expectsExternalAssets || runtimeReadable
                ? DoctorStatus::Passed
                : DoctorStatus::Failed,
            "Crossa",
            "Runtime assets",
            !expectsExternalAssets
                ? "embedded"
                : runtimeReadable ? "available" : "not readable",
            "Reinstall Crossa so Android runtime assets are present."
        });
        results.push_back(DoctorResult{
            !expectsExternalAssets || templatesReadable
                ? DoctorStatus::Passed
                : DoctorStatus::Failed,
            "Crossa",
            "Build templates",
            !expectsExternalAssets
                ? "embedded"
                : templatesReadable ? "available" : "not readable",
            "Reinstall Crossa so Android build templates are present."
        });
        return results;
    }

    // Returns structured host results without writing output.
    vector<DoctorResult> HostCheck::run() const {
        return vector<DoctorResult>{
            DoctorResult{
                DoctorStatus::Passed,
                "Host",
                "Operating system",
                DoctorCheckUtils::operatingSystemName(),
                ""
            },
            DoctorResult{
                DoctorStatus::Passed,
                "Host",
                "Architecture",
                DoctorCheckUtils::architectureName(),
                ""
            }
        };
    }

    // Returns structured Android SDK results without writing output.
    vector<DoctorResult> AndroidSdkCheck::run() const {
        vector<DoctorResult> results;
        const optional<filesystem::path> sdkDirectory =
            DoctorCheckUtils::androidHome();
        if (!sdkDirectory.has_value()) {
            results.push_back(DoctorResult{
                DoctorStatus::Failed,
                "Android",
                "ANDROID_HOME",
                "not configured",
                "Set ANDROID_HOME to the Android SDK directory."
            });
            results.push_back(DoctorResult{
                DoctorStatus::Failed,
                "Android",
                "Android SDK",
                "not available",
                "Install the Android SDK platform required by Crossa."
            });
            results.push_back(DoctorResult{
                DoctorStatus::Warning,
                "Android",
                "sdkmanager",
                "not found",
                "Install Android SDK Command-line Tools if you need SDK package management."
            });
            return results;
        }

        const bool sdkDirectoryExists =
            DoctorCheckUtils::isReadableDirectory(sdkDirectory.value());
        results.push_back(DoctorResult{
            sdkDirectoryExists ? DoctorStatus::Passed : DoctorStatus::Failed,
            "Android",
            "ANDROID_HOME",
            sdkDirectoryExists ? sdkDirectory->string() : "not a directory",
            "Set ANDROID_HOME to a readable Android SDK directory."
        });

        string missing;
        const bool hasSdkStructure = sdkDirectoryExists &&
            DoctorCheckUtils::hasRequiredSdkStructure(
                sdkDirectory.value(),
                missing
            );
        results.push_back(DoctorResult{
            hasSdkStructure ? DoctorStatus::Passed : DoctorStatus::Failed,
            "Android",
            "Android SDK",
            hasSdkStructure ? "available" : missing,
            "Install the Android SDK platform and build tools required by Crossa."
        });

        const optional<filesystem::path> sdkManager =
            sdkDirectoryExists
                ? DoctorCheckUtils::sdkManagerPath(sdkDirectory.value())
                : nullopt;
        results.push_back(DoctorResult{
            sdkManager.has_value()
                ? DoctorStatus::Passed
                : DoctorStatus::Warning,
            "Android",
            "sdkmanager",
            sdkManager.has_value() ? "available" : "not found",
            "Install Android SDK Command-line Tools if you need SDK package management."
        });
        return results;
    }

    // Returns structured Android NDK results without writing output.
    vector<DoctorResult> AndroidNdkCheck::run() const {
        const string requiredNdk =
            packaging::android::AndroidBuildRequirements::ndkVersion();
        const optional<filesystem::path> sdkDirectory =
            DoctorCheckUtils::androidHome();
        if (!sdkDirectory.has_value()) {
            return vector<DoctorResult>{
                DoctorResult{
                    DoctorStatus::Failed,
                    "Android",
                    "Android NDK",
                    "requires " + requiredNdk + ", ANDROID_HOME missing",
                    "Set ANDROID_HOME and install the required Android NDK version."
                }
            };
        }

        const filesystem::path ndkDirectory =
            sdkDirectory.value() / "ndk" / requiredNdk;
        const bool ndkAvailable =
            DoctorCheckUtils::isReadableDirectory(ndkDirectory);
        return vector<DoctorResult>{
            DoctorResult{
                ndkAvailable ? DoctorStatus::Passed : DoctorStatus::Failed,
                "Android",
                "Android NDK",
                ndkAvailable
                    ? requiredNdk
                    : "required " + requiredNdk + ", not installed",
                "Install the required Android NDK version."
            }
        };
    }

    // Returns structured CMake results without writing output.
    vector<DoctorResult> CMakeCheck::run() const {
        const string requiredVersion =
            packaging::android::AndroidBuildRequirements::
                cmakeMinimumVersion();
        vector<filesystem::path> candidates;
        const optional<filesystem::path> sdkDirectory =
            DoctorCheckUtils::androidHome();
        if (sdkDirectory.has_value()) {
            candidates = DoctorCheckUtils::sdkCMakeExecutables(
                sdkDirectory.value()
            );
        }
        const optional<filesystem::path> pathCMake =
            DoctorCheckUtils::findExecutable("cmake");
        if (pathCMake.has_value()) {
            candidates.push_back(pathCMake.value());
        }

        string bestRejectedVersion;
        for (const filesystem::path& candidate : candidates) {
            const ProcessResult process =
                ProcessRunner::run({candidate.string(), "--version"});
            if (!process.started || process.exitCode != 0) {
                continue;
            }
            const string version =
                DoctorCheckUtils::parseCMakeVersion(process.output);
            if (version.empty()) {
                continue;
            }
            if (DoctorCheckUtils::compareVersions(
                    version,
                    requiredVersion
                ) >= 0) {
                return vector<DoctorResult>{
                    DoctorResult{
                        DoctorStatus::Passed,
                        "Android",
                        "CMake",
                        version,
                        ""
                    }
                };
            }
            bestRejectedVersion = version;
        }

        return vector<DoctorResult>{
            DoctorResult{
                DoctorStatus::Failed,
                "Android",
                "CMake",
                bestRejectedVersion.empty()
                    ? "not found"
                    : "found " + bestRejectedVersion + ", requires " +
                        requiredVersion,
                "Install CMake " + requiredVersion +
                    " or newer for Android native builds."
            }
        };
    }

    // Returns structured Ninja results without writing output.
    vector<DoctorResult> NinjaCheck::run() const {
        vector<filesystem::path> candidates;
        const optional<filesystem::path> sdkDirectory =
            DoctorCheckUtils::androidHome();
        if (sdkDirectory.has_value()) {
            candidates = DoctorCheckUtils::sdkNinjaExecutables(
                sdkDirectory.value()
            );
        }
        const optional<filesystem::path> pathNinja =
            DoctorCheckUtils::findExecutable("ninja");
        if (pathNinja.has_value()) {
            candidates.push_back(pathNinja.value());
        }

        for (const filesystem::path& candidate : candidates) {
            const ProcessResult process =
                ProcessRunner::run({candidate.string(), "--version"});
            if (process.started && process.exitCode == 0) {
                return vector<DoctorResult>{
                    DoctorResult{
                        DoctorStatus::Passed,
                        "Android",
                        "Ninja",
                        DoctorCheckUtils::firstTrimmedLine(process.output),
                        ""
                    }
                };
            }
        }

        return vector<DoctorResult>{
            DoctorResult{
                DoctorStatus::Failed,
                "Android",
                "Ninja",
                "not found",
                "Install Ninja so Android CMake builds can execute."
            }
        };
    }

    // Returns structured Java results without writing output.
    vector<DoctorResult> JavaCheck::run() const {
        const int requiredMajor =
            packaging::android::AndroidBuildRequirements::
                javaToolchainVersion();
        vector<filesystem::path> candidates;
        const optional<string> javaHome =
            DoctorCheckUtils::environmentValue("JAVA_HOME");
        if (javaHome.has_value()) {
            const filesystem::path javaHomeCandidate =
                filesystem::path(javaHome.value()) / "bin" / "java";
            if (DoctorCheckUtils::isExecutable(javaHomeCandidate)) {
                candidates.push_back(javaHomeCandidate);
            }
        }
        const optional<filesystem::path> pathJava =
            DoctorCheckUtils::findExecutable("java");
        if (pathJava.has_value()) {
            candidates.push_back(pathJava.value());
        }

        string rejectedVersion;
        for (const filesystem::path& candidate : candidates) {
            const ProcessResult process =
                ProcessRunner::run({candidate.string(), "-version"});
            if (!process.started || process.exitCode != 0) {
                continue;
            }
            const string version =
                DoctorCheckUtils::parseJavaVersion(process.output);
            if (version.empty()) {
                continue;
            }
            const int major = DoctorCheckUtils::javaMajorVersion(version);
            if (major >= requiredMajor) {
                return vector<DoctorResult>{
                    DoctorResult{
                        DoctorStatus::Passed,
                        "Android",
                        "Java",
                        version,
                        ""
                    }
                };
            }
            rejectedVersion = version;
        }

        return vector<DoctorResult>{
            DoctorResult{
                DoctorStatus::Failed,
                "Android",
                "Java",
                rejectedVersion.empty()
                    ? "not found"
                    : "found " + rejectedVersion + ", requires " +
                        to_string(requiredMajor) + "+",
                "Install Java " + to_string(requiredMajor) +
                    " or newer and set JAVA_HOME when needed."
            }
        };
    }

    // Returns structured storage results without writing output.
    vector<DoctorResult> StorageCheck::run() const {
        vector<DoctorResult> results;
        const optional<filesystem::path> cacheDirectory =
            DoctorCheckUtils::cacheDirectory();
        if (!cacheDirectory.has_value()) {
            results.push_back(DoctorResult{
                DoctorStatus::Failed,
                "Storage",
                "Crossa cache",
                "not available",
                "Set HOME or CROSSA_CACHE_DIR to a writable location."
            });
        } else {
            const bool cacheWritable =
                DoctorCheckUtils::canUseWritablePath(cacheDirectory.value());
            results.push_back(DoctorResult{
                cacheWritable ? DoctorStatus::Passed : DoctorStatus::Failed,
                "Storage",
                "Crossa cache",
                cacheWritable ? "writable" : "not writable",
                "Use a writable Crossa cache directory."
            });
        }

        error_code error;
        const filesystem::path temporaryDirectory =
            filesystem::temp_directory_path(error);
        const bool temporaryWritable = !error &&
            DoctorCheckUtils::canUseWritablePath(temporaryDirectory);
        results.push_back(DoctorResult{
            temporaryWritable ? DoctorStatus::Passed : DoctorStatus::Failed,
            "Storage",
            "Temporary directory",
            temporaryWritable ? "writable" : "not writable",
            "Configure the system temporary directory to a writable location."
        });
        return results;
    }

}
