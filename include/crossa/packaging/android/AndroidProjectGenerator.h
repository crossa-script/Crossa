#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include "crossa/compiler/generators/kotlin/KotlinGeneratedSource.h"
#include "crossa/compiler/ir/Program.h"

namespace crossa::packaging::android {

// Stores the tool versions selected for one generated Android library project.
struct AndroidBuildVersions final {
    std::string ndkVersion;
    std::string gradleVersion;
    std::string kotlinVersion;
};

// Creates a deterministic Android library project around generated Kotlin APIs.
// generate() writes Gradle, manifest, native-build, configuration, and API files.
class AndroidProjectGenerator final {
public:
    // Writes a complete Android library project into an empty output directory.
    void generate(
        const std::vector<compiler::generators::kotlin::KotlinGeneratedSource>&
            kotlinSources,
        const std::vector<const compiler::ir::Program*>& programs,
        const std::optional<std::string>& packageName,
        const std::filesystem::path& outputDirectory,
        const AndroidBuildVersions& buildVersions
    ) const;

private:
    // Resolves the generated Android package from validated configuration input.
    [[nodiscard]] static std::string resolvePackageName(
        const std::optional<std::string>& packageName
    );

    // Converts a dot-separated Kotlin package into its source directory path.
    [[nodiscard]] static std::filesystem::path packagePath(
        const std::string& packageName
    );

    // Creates a directory or reports a deterministic output diagnostic.
    static void createDirectory(const std::filesystem::path& directory);

    // Writes one generated project file without partially replacing its target.
    static void writeFile(
        const std::filesystem::path& outputPath,
        const std::string& content
    );

    // Removes outputs listed by the previous generated Kotlin manifest only.
    static void removeStaleKotlinSources(
        const std::filesystem::path& kotlinDirectory,
        const std::vector<std::string>& currentPaths
    );

    // Writes the stable manifest used to remove stale Kotlin source outputs.
    static void writeKotlinManifest(
        const std::filesystem::path& kotlinDirectory,
        const std::vector<std::string>& currentPaths
    );

    // Copies one trusted generated-project binary or script and preserves permissions.
    static void copyFile(
        const std::filesystem::path& sourcePath,
        const std::filesystem::path& outputPath,
        bool executable
    );

    // Copies the trusted Gradle Wrapper and selects its requested distribution version.
    static void writeGradleWrapper(
        const std::filesystem::path& outputDirectory,
        const std::string& gradleVersion
    );

    // Writes deterministic Android OpenSSL, curl, and CA dependency provisioning.
    static void writeAndroidDependencies(
        const std::filesystem::path& outputDirectory
    );

    // Writes the Gradle project and Android library build definitions.
    static void writeBuildFiles(
        const std::filesystem::path& outputDirectory,
        const std::string& packageName,
        const AndroidBuildVersions& buildVersions
    );

    // Writes the generated configuration API and JNI bridge declarations.
    static void writeRuntimeApi(
        const std::filesystem::path& sourceDirectory,
        const std::string& packageName
    );

    // Writes the Android manifest and native CMake project source.
    static void writeNativeBuildFiles(
        const std::filesystem::path& outputDirectory,
        const std::string& packageName,
        const std::vector<const compiler::ir::Program*>& programs
    );
};

}
