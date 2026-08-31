#include "crossa/packaging/android/AndroidProjectGenerator.h"

#include <fstream>
#include <stdexcept>
#include <system_error>

#include "crossa/packaging/android/AndroidBuildRequirements.h"
#include "crossa/compiler/generators/native/NativeProgramGenerator.h"

using namespace std;

namespace crossa::packaging::android {

    // Writes a complete Android library project into an empty output directory.
    void AndroidProjectGenerator::generate(
        const vector<compiler::generators::kotlin::KotlinGeneratedSource>&
            kotlinSources,
        const vector<const compiler::ir::Program*>& programs,
        const optional<string>& packageName,
        const filesystem::path& outputDirectory
    ) const {
        const string resolvedPackageName = resolvePackageName(packageName);
        createDirectory(outputDirectory);
        writeBuildFiles(outputDirectory, resolvedPackageName);
        writeNativeBuildFiles(outputDirectory, resolvedPackageName, programs);

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
                AndroidBuildRequirements::recommendedNdkVersion() +
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
            sourceDirectory / "CrossaState.kt",
            "package " + packageName + "\n\n"
            "public sealed interface CrossaState<out T> {\n"
            "    public data class Success<T>(public val data: T) : CrossaState<T>\n"
            "    public data class Failed(public val error: CrossaError) : CrossaState<Nothing>\n"
            "    public data object Cancelled : CrossaState<Nothing>\n"
            "}\n\n"
            "public data class CrossaError(\n"
            "    public val domain: Int,\n"
            "    public val code: Int,\n"
            "    public val message: String,\n"
            "    public val retryable: Boolean\n"
            ")\n"
        );
        writeFile(
            sourceDirectory / "CrossaNativeResult.kt",
            "package " + packageName + "\n\n"
            "public class CrossaNativeResult internal constructor(\n"
            "    private var handle: Long\n"
            ") : AutoCloseable {\n"
            "    internal fun requireHandle(): Long = check(handle != 0L) { \"Crossa native result is closed.\" }.let { handle }\n"
            "    internal fun rootModelHandle(): Long = CrossaNativeBridge.rootModelHandle(this)\n"
            "    internal fun intValue(): Int = CrossaNativeBridge.resultInt(this)\n"
            "    internal fun longValue(): Long = CrossaNativeBridge.resultLong(this)\n"
            "    internal fun doubleValue(): Double = CrossaNativeBridge.resultDouble(this)\n"
            "    internal fun stringValue(): String = CrossaNativeBridge.resultString(this)\n"
            "    internal fun booleanValue(): Boolean = CrossaNativeBridge.resultBoolean(this)\n"
            "    override fun close() { if (handle != 0L) { CrossaNativeBridge.releaseResult(handle); handle = 0L } }\n"
            "}\n\n"
            "public class CrossaNativeList<T> internal constructor(\n"
            "    private val owner: CrossaNativeResult,\n"
            "    private val factory: (Long) -> T\n"
            ") : AbstractList<T>(), AutoCloseable {\n"
            "    override val size: Int get() = CrossaNativeBridge.listSize(owner)\n"
            "    override fun get(index: Int): T = factory(CrossaNativeBridge.listModelHandle(owner, index))\n"
            "    override fun close() { owner.close() }\n"
            "}\n"
        );
        writeFile(
            sourceDirectory / "CrossaArgument.kt",
            "package " + packageName + "\n\n"
            "public sealed class CrossaArgument private constructor() {\n"
            "    internal data class IntValue(val value: Int) : CrossaArgument()\n"
            "    internal data class LongValue(val value: Long) : CrossaArgument()\n"
            "    internal data class DoubleValue(val value: Double) : CrossaArgument()\n"
            "    internal data class StringValue(val value: String) : CrossaArgument()\n"
            "    internal data class BooleanValue(val value: Boolean) : CrossaArgument()\n"
            "    public companion object {\n"
            "        public fun from(value: Int): CrossaArgument = IntValue(value)\n"
            "        public fun from(value: Long): CrossaArgument = LongValue(value)\n"
            "        public fun from(value: Double): CrossaArgument = DoubleValue(value)\n"
            "        public fun from(value: String): CrossaArgument = StringValue(value)\n"
            "        public fun from(value: Boolean): CrossaArgument = BooleanValue(value)\n"
            "    }\n"
            "}\n"
        );
        writeFile(
            sourceDirectory / "CrossaNativeBridge.kt",
            "package " + packageName + "\n\n"
            "internal object CrossaNativeBridge {\n"
            "    init { System.loadLibrary(\"crossa_runtime\") }\n"
            "    internal fun invokeAsync(operationId: Long, arguments: Array<CrossaArgument>) { nativeInvokeAsync(operationId, arguments) }\n"
            "    internal fun <T> invokeAsyncAfter(operationId: Long, arguments: Array<CrossaArgument>, mapper: (CrossaNativeResult) -> T, onState: (CrossaState<T>) -> Unit) {\n"
            "        nativeInvokeAsyncAfter(operationId, arguments) { state ->\n"
            "            when (state) {\n"
            "                is CrossaState.Success -> onState(CrossaState.Success(mapper(state.data)))\n"
            "                is CrossaState.Failed -> onState(state)\n"
            "                CrossaState.Cancelled -> onState(CrossaState.Cancelled)\n"
            "            }\n"
            "        }\n"
            "    }\n"
            "    internal fun releaseResult(handle: Long) { nativeReleaseResult(handle) }\n"
            "    internal fun listSize(result: CrossaNativeResult): Int = nativeListSize(result.requireHandle())\n"
            "    internal fun listModelHandle(result: CrossaNativeResult, index: Int): Long = nativeListModelHandle(result.requireHandle(), index)\n"
            "    internal fun rootModelHandle(result: CrossaNativeResult): Long = nativeRootModelHandle(result.requireHandle())\n"
            "    internal fun resultInt(result: CrossaNativeResult): Int = nativeResultInt(result.requireHandle())\n"
            "    internal fun resultLong(result: CrossaNativeResult): Long = nativeResultLong(result.requireHandle())\n"
            "    internal fun resultDouble(result: CrossaNativeResult): Double = nativeResultDouble(result.requireHandle())\n"
            "    internal fun resultString(result: CrossaNativeResult): String = nativeResultString(result.requireHandle())\n"
            "    internal fun resultBoolean(result: CrossaNativeResult): Boolean = nativeResultBoolean(result.requireHandle())\n"
            "    internal fun modelInt(result: CrossaNativeResult, model: Long, field: Int): Int = nativeModelInt(result.requireHandle(), model, field)\n"
            "    internal fun modelLong(result: CrossaNativeResult, model: Long, field: Int): Long = nativeModelLong(result.requireHandle(), model, field)\n"
            "    internal fun modelDouble(result: CrossaNativeResult, model: Long, field: Int): Double = nativeModelDouble(result.requireHandle(), model, field)\n"
            "    internal fun modelString(result: CrossaNativeResult, model: Long, field: Int): String = nativeModelString(result.requireHandle(), model, field)\n"
            "    internal fun modelBoolean(result: CrossaNativeResult, model: Long, field: Int): Boolean = nativeModelBoolean(result.requireHandle(), model, field)\n"
            "    private external fun nativeInvokeAsync(operationId: Long, arguments: Array<CrossaArgument>)\n"
            "    private external fun nativeInvokeAsyncAfter(operationId: Long, arguments: Array<CrossaArgument>, callback: (CrossaState<CrossaNativeResult>) -> Unit)\n"
            "    private external fun nativeReleaseResult(handle: Long)\n"
            "    private external fun nativeListSize(result: Long): Int\n"
            "    private external fun nativeListModelHandle(result: Long, index: Int): Long\n"
            "    private external fun nativeRootModelHandle(result: Long): Long\n"
            "    private external fun nativeResultInt(result: Long): Int\n"
            "    private external fun nativeResultLong(result: Long): Long\n"
            "    private external fun nativeResultDouble(result: Long): Double\n"
            "    private external fun nativeResultString(result: Long): String\n"
            "    private external fun nativeResultBoolean(result: Long): Boolean\n"
            "    private external fun nativeModelInt(result: Long, model: Long, field: Int): Int\n"
            "    private external fun nativeModelLong(result: Long, model: Long, field: Int): Long\n"
            "    private external fun nativeModelDouble(result: Long, model: Long, field: Int): Double\n"
            "    private external fun nativeModelString(result: Long, model: Long, field: Int): String\n"
            "    private external fun nativeModelBoolean(result: Long, model: Long, field: Int): Boolean\n"
            "}\n"
        );
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
        const string& packageName,
        const vector<const compiler::ir::Program*>& programs
    ) {
        writeFile(
            outputDirectory / "library" / "src" / "main" / "AndroidManifest.xml",
            "<manifest xmlns:android=\"http://schemas.android.com/apk/res/android\" />\n"
        );
        compiler::generators::native::NativeProgramGenerator nativeGenerator;
        string operationHeaders;
        for (const compiler::ir::Program* program : programs) {
            if (program == nullptr) {
                throw runtime_error("Android generation requires a linked IR program.");
            }
            operationHeaders += nativeGenerator.generateOperationHeader(*program);
        }
        writeFile(
            outputDirectory / "library" / "src" / "main" / "cpp" /
                "CrossaGeneratedOperations.h",
            operationHeaders
        );
        if (programs.size() != 1) {
            throw runtime_error("Android native generation requires one linked program.");
        }
        writeFile(
            outputDirectory / "library" / "src" / "main" / "cpp" /
                "CrossaGeneratedProgram.h",
            nativeGenerator.generateProgramHeader()
        );
        writeFile(
            outputDirectory / "library" / "src" / "main" / "cpp" /
                "CrossaGeneratedProgram.cpp",
            nativeGenerator.generateProgramSource(*programs.front())
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
