#include "crossa/packaging/android/AndroidProjectGenerator.h"

#include <fstream>
#include <stdexcept>
#include <system_error>

#include "crossa/packaging/android/AndroidBuildRequirements.h"

using namespace std;

namespace crossa::packaging::android {

    // Writes a complete Android library project into an empty output directory.
    void AndroidProjectGenerator::generate(
        const vector<compiler::generators::kotlin::KotlinGeneratedSource>&
            kotlinSources,
        const optional<string>& packageName,
        const filesystem::path& outputDirectory
    ) const {
        const string resolvedPackageName = resolvePackageName(packageName);
        createDirectory(outputDirectory);
        writeBuildFiles(outputDirectory, resolvedPackageName);
        writeNativeBuildFiles(outputDirectory, resolvedPackageName);

        const filesystem::path kotlinDirectory = outputDirectory / "library" /
            "src" / "main" / "kotlin" / packagePath(resolvedPackageName);
        createDirectory(kotlinDirectory);
        writeRuntimeApi(kotlinDirectory, resolvedPackageName);
        for (const auto& source : kotlinSources) {
            writeFile(kotlinDirectory / source.getFileName(), source.getContent());
        }
    }

    // Resolves the generated Android package from validated configuration input.
    string AndroidProjectGenerator::resolvePackageName(
        const optional<string>& packageName
    ) {
        if (!packageName.has_value()) {
            return "io.crossa.generated";
        }
        if (packageName->empty()) {
            throw runtime_error("Android config packageName must not be empty.");
        }
        return *packageName;
    }

    // Converts a dot-separated Kotlin package into its source directory path.
    filesystem::path AndroidProjectGenerator::packagePath(
        const string& packageName
    ) {
        filesystem::path result;
        size_t segmentStart = 0;
        while (segmentStart < packageName.size()) {
            const size_t separator = packageName.find('.', segmentStart);
            result /= packageName.substr(
                segmentStart,
                separator == string::npos
                    ? string::npos
                    : separator - segmentStart
            );
            if (separator == string::npos) {
                break;
            }
            segmentStart = separator + 1;
        }
        return result;
    }

    // Creates a directory or reports a deterministic output diagnostic.
    void AndroidProjectGenerator::createDirectory(const filesystem::path& directory) {
        error_code error;
        filesystem::create_directories(directory, error);
        if (error || !filesystem::is_directory(directory, error)) {
            throw runtime_error(
                "Unable to create Android output directory: " + directory.string()
            );
        }
    }

    // Writes one generated project file without partially replacing its target.
    void AndroidProjectGenerator::writeFile(
        const filesystem::path& outputPath,
        const string& content
    ) {
        createDirectory(outputPath.parent_path());
        filesystem::path temporaryPath = outputPath;
        temporaryPath += ".tmp";
        ofstream output(temporaryPath, ios::binary | ios::trunc);
        if (!output.is_open()) {
            throw runtime_error(
                "Unable to write Android generated file: " + temporaryPath.string()
            );
        }
        output.write(content.data(), static_cast<streamsize>(content.size()));
        output.close();
        if (!output) {
            error_code error;
            filesystem::remove(temporaryPath, error);
            throw runtime_error(
                "Unable to finish Android generated file: " + temporaryPath.string()
            );
        }
        error_code error;
        if (filesystem::exists(outputPath, error)) {
            filesystem::remove(outputPath, error);
            if (error) {
                filesystem::remove(temporaryPath, error);
                throw runtime_error(
                    "Unable to replace Android generated file: " +
                    outputPath.string()
                );
            }
        }
        filesystem::rename(temporaryPath, outputPath, error);
        if (error) {
            filesystem::remove(temporaryPath, error);
            throw runtime_error(
                "Unable to finalize Android generated file: " + outputPath.string()
            );
        }
    }

    // Writes the Gradle project and Android library build definitions.
    void AndroidProjectGenerator::writeBuildFiles(
        const filesystem::path& outputDirectory,
        const string& packageName
    ) {
        writeFile(
            outputDirectory / "settings.gradle.kts",
            "pluginManagement { repositories { google(); mavenCentral(); gradlePluginPortal() } }\n"
            "dependencyResolutionManagement { repositoriesMode.set(RepositoriesMode.FAIL_ON_PROJECT_REPOS); repositories { google(); mavenCentral() } }\n"
            "rootProject.name = \"CrossaGenerated\"\n"
            "include(\":library\")\n"
        );
        writeFile(
            outputDirectory / "build.gradle.kts",
            "plugins {\n"
            "    id(\"com.android.library\") version \"" +
                AndroidBuildRequirements::androidGradlePluginVersion() +
                "\" apply false\n"
            "    kotlin(\"android\") version \"" +
                AndroidBuildRequirements::kotlinAndroidPluginVersion() +
                "\" apply false\n"
            "}\n"
        );
        writeFile(
            outputDirectory / "gradle.properties",
            "org.gradle.jvmargs=-Xmx2048m -Dfile.encoding=UTF-8\n"
            "android.useAndroidX=true\n"
            "android.nonTransitiveRClass=true\n"
        );
        writeFile(
            outputDirectory / "library" / "build.gradle.kts",
            "plugins {\n"
            "    id(\"com.android.library\")\n"
            "    kotlin(\"android\")\n"
            "}\n\n"
            "android {\n"
            "    namespace = \"" + packageName + "\"\n"
            "    compileSdk = " +
                to_string(AndroidBuildRequirements::compileSdkVersion()) +
                "\n\n"
            "    ndkVersion = \"" +
                AndroidBuildRequirements::ndkVersion() +
                "\"\n\n"
            "    defaultConfig {\n"
            "        minSdk = 23\n"
            "        externalNativeBuild { cmake { cppFlags += listOf(\"-std=c++20\", \"-O3\") } }\n"
            "    }\n\n"
            "    externalNativeBuild { cmake { path = file(\"src/main/cpp/CMakeLists.txt\") } }\n"
            "}\n\n"
            "kotlin { jvmToolchain(" +
                to_string(AndroidBuildRequirements::javaToolchainVersion()) +
                ") }\n"
        );
    }

    // Writes the generated configuration API and JNI bridge declarations.
    void AndroidProjectGenerator::writeRuntimeApi(
        const filesystem::path& sourceDirectory,
        const string& packageName
    ) {
        writeFile(
            sourceDirectory / "CrossaConfigurationOverrides.kt",
            "package " + packageName + "\n\n"
            "public data class CrossaConfigurationOverrides(\n"
            "    public val baseUrl: String? = null,\n"
            "    public val timeoutRequest: Long? = null,\n"
            "    public val commonHeaders: List<Header>? = null,\n"
            "    public val interceptor: Interceptor? = null,\n"
            "    public val workerThreads: Int? = null,\n"
            "    public val maxQueuedTasks: Int? = null,\n"
            "    public val maxResponseBytes: Long? = null,\n"
            "    public val maxJsonDepth: Int? = null,\n"
            "    public val followRedirects: Boolean? = null,\n"
            "    public val uploadProgress: Boolean? = null,\n"
            "    public val downloadStreaming: Boolean? = null,\n"
            "    public val requestCoalescing: Boolean? = null\n"
            ") {\n"
            "    public data class Header(public val name: String, public val value: String)\n\n"
            "    public data class Interceptor(\n"
            "        public val enabled: Boolean? = null,\n"
            "        public val logRequests: Boolean? = null,\n"
            "        public val logResponses: Boolean? = null,\n"
            "        public val logHeaders: Boolean? = null,\n"
            "        public val logBody: Boolean? = null,\n"
            "        public val excludedLogHeaders: List<String>? = null\n"
            "    )\n"
            "}\n"
        );
        writeFile(
            sourceDirectory / "CrossaRuntime.kt",
            "package " + packageName + "\n\n"
            "public object CrossaRuntime {\n"
            "    init { System.loadLibrary(\"crossa_runtime\") }\n\n"
            "    public fun configure(overrides: CrossaConfigurationOverrides) {\n"
            "        nativeConfigure(overrides)\n"
            "    }\n\n"
            "    private external fun nativeConfigure(overrides: CrossaConfigurationOverrides)\n"
            "}\n"
        );
    }

    // Writes the Android manifest and native CMake project source.
    void AndroidProjectGenerator::writeNativeBuildFiles(
        const filesystem::path& outputDirectory,
        const string& packageName
    ) {
        writeFile(
            outputDirectory / "library" / "src" / "main" / "AndroidManifest.xml",
            "<manifest xmlns:android=\"http://schemas.android.com/apk/res/android\" />\n"
        );
        writeFile(
            outputDirectory / "library" / "src" / "main" / "cpp" / "CMakeLists.txt",
            "cmake_minimum_required(VERSION " +
                AndroidBuildRequirements::cmakeMinimumVersion() +
                ")\n"
            "project(crossa_runtime LANGUAGES CXX)\n\n"
            "add_library(crossa_runtime SHARED crossa_runtime.cpp)\n"
            "target_compile_features(crossa_runtime PRIVATE cxx_std_20)\n"
            "target_compile_options(crossa_runtime PRIVATE -O3)\n"
            "target_link_options(crossa_runtime PRIVATE -Wl,-z,max-page-size=16384 -Wl,-z,common-page-size=16384)\n"
        );
        const string packagePathValue = [&packageName]() {
            string result;
            for (const char value : packageName) {
                result += value == '.' ? '_' : value;
            }
            return result;
        }();
        writeFile(
            outputDirectory / "library" / "src" / "main" / "cpp" / "crossa_runtime.cpp",
            "#include <jni.h>\n\n"
            "extern \"C\" JNIEXPORT void JNICALL\n"
            "Java_" + packagePathValue + "_CrossaRuntime_nativeConfigure(\n"
            "    JNIEnv*,\n"
            "    jobject,\n"
            "    jobject\n"
            ") {}\n"
        );
    }

}
