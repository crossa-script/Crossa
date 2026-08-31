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

    // Copies one trusted generated-project binary or script and preserves permissions.
    void AndroidProjectGenerator::copyFile(
        const filesystem::path& sourcePath,
        const filesystem::path& outputPath,
        bool executable
    ) {
        error_code error;
        if (!filesystem::is_regular_file(sourcePath, error) || error) {
            throw runtime_error(
                "Missing trusted Android project template file: " +
                sourcePath.string()
            );
        }
        createDirectory(outputPath.parent_path());
        filesystem::copy_file(
            sourcePath,
            outputPath,
            filesystem::copy_options::overwrite_existing,
            error
        );
        if (error) {
            throw runtime_error(
                "Unable to copy Android generated file: " + outputPath.string()
            );
        }
        if (executable) {
            filesystem::permissions(
                outputPath,
                filesystem::perms::owner_exec | filesystem::perms::group_exec |
                    filesystem::perms::others_exec,
                filesystem::perm_options::add,
                error
            );
            if (error) {
                throw runtime_error(
                    "Unable to make Android Gradle Wrapper executable: " +
                    outputPath.string()
                );
            }
        }
    }

    // Copies the repository-pinned Gradle Wrapper into a generated project.
    void AndroidProjectGenerator::writeGradleWrapper(
        const filesystem::path& outputDirectory
    ) {
#ifdef CROSSA_SOURCE_DIRECTORY
        const filesystem::path sourceRoot(CROSSA_SOURCE_DIRECTORY);
        const filesystem::path wrapperDirectory = sourceRoot.parent_path() /
            "android-example" / "gradle" / "wrapper";
        copyFile(
            sourceRoot.parent_path() / "android-example" / "gradlew",
            outputDirectory / "gradlew",
            true
        );
        copyFile(
            sourceRoot.parent_path() / "android-example" / "gradlew.bat",
            outputDirectory / "gradlew.bat",
            false
        );
        copyFile(
            wrapperDirectory / "gradle-wrapper.jar",
            outputDirectory / "gradle" / "wrapper" / "gradle-wrapper.jar",
            false
        );
        copyFile(
            wrapperDirectory / "gradle-wrapper.properties",
            outputDirectory / "gradle" / "wrapper" /
                "gradle-wrapper.properties",
            false
        );
#else
        throw runtime_error("Crossa Android generation requires the trusted Gradle Wrapper template.");
#endif
    }

    // Writes deterministic Android OpenSSL, curl, and CA dependency provisioning.
    void AndroidProjectGenerator::writeAndroidDependencies(
        const filesystem::path& outputDirectory
    ) {
        const filesystem::path nativeDirectory = outputDirectory / "library" /
            "src" / "main" / "cpp";
        writeFile(
            nativeDirectory / "CrossaOpenSslInstall.cmake",
            "if(NOT DEFINED source OR NOT DEFINED binary OR NOT DEFINED destination)\n"
            "    message(FATAL_ERROR \"Crossa OpenSSL staging requires source, binary, and destination paths.\")\n"
            "endif()\n\n"
            "file(MAKE_DIRECTORY \"${destination}/lib\")\n"
            "file(COPY \"${source}/include/\" DESTINATION \"${destination}/include\")\n"
            "file(COPY \"${binary}/include/\" DESTINATION \"${destination}/include\")\n"
            "file(COPY \"${binary}/libssl.a\" \"${binary}/libcrypto.a\" DESTINATION \"${destination}/lib\")\n"
        );
        writeFile(
            nativeDirectory / "CrossaAndroidDependencies.cmake",
            "include(ExternalProject)\n\n"
            "if(NOT ANDROID OR NOT CMAKE_ANDROID_NDK)\n"
            "    message(FATAL_ERROR \"Crossa Android dependencies require the Android NDK CMake toolchain.\")\n"
            "endif()\n\n"
            "set(CROSSA_ANDROID_THIRD_PARTY_DIRECTORY \"${CMAKE_BINARY_DIR}/crossa-third-party\")\n"
            "set(CROSSA_ANDROID_DOWNLOAD_DIRECTORY \"${CROSSA_ANDROID_THIRD_PARTY_DIRECTORY}/downloads\")\n"
            "set(CROSSA_ANDROID_CA_FILE \"${CROSSA_ANDROID_DOWNLOAD_DIRECTORY}/cacert-" +
                AndroidBuildRequirements::caBundleVersion() + ".pem\")\n"
            "file(MAKE_DIRECTORY \"${CROSSA_ANDROID_DOWNLOAD_DIRECTORY}\")\n"
            "file(DOWNLOAD\n"
            "    \"" + AndroidBuildRequirements::caBundleUrl() + "\"\n"
            "    \"${CROSSA_ANDROID_CA_FILE}\"\n"
            "    EXPECTED_HASH \"SHA256=" +
                AndroidBuildRequirements::caBundleSha256() + "\"\n"
            "    TLS_VERIFY ON\n"
            ")\n"
            "file(READ \"${CROSSA_ANDROID_CA_FILE}\" CROSSA_ANDROID_CA_CONTENT)\n"
            "file(WRITE \"${CMAKE_CURRENT_BINARY_DIR}/CrossaAndroidCaBundle.h\" [=[#pragma once\n"
            "#include <cstddef>\n\n"
            "namespace crossa::network::transport {\n\n"
            "inline constexpr char kCrossaAndroidCaBundle[] = R\"CROSSA_CA(\n]=])\n"
            "file(APPEND \"${CMAKE_CURRENT_BINARY_DIR}/CrossaAndroidCaBundle.h\" \"${CROSSA_ANDROID_CA_CONTENT}\")\n"
            "file(APPEND \"${CMAKE_CURRENT_BINARY_DIR}/CrossaAndroidCaBundle.h\" [=[)CROSSA_CA\";\n"
            "inline constexpr size_t kCrossaAndroidCaBundleSize = sizeof(kCrossaAndroidCaBundle) - 1;\n\n"
            "}\n]=])\n\n"
            "file(GLOB CROSSA_ANDROID_TOOLCHAIN_BIN \"${CMAKE_ANDROID_NDK}/toolchains/llvm/prebuilt/*/bin\")\n"
            "list(LENGTH CROSSA_ANDROID_TOOLCHAIN_BIN CROSSA_ANDROID_TOOLCHAIN_COUNT)\n"
            "if(CROSSA_ANDROID_TOOLCHAIN_COUNT EQUAL 0)\n"
            "    message(FATAL_ERROR \"Unable to find the Android NDK LLVM toolchain.\")\n"
            "endif()\n"
            "list(GET CROSSA_ANDROID_TOOLCHAIN_BIN 0 CROSSA_ANDROID_TOOLCHAIN_BIN)\n"
            "set(CROSSA_ANDROID_TOOL_PATH \"${CROSSA_ANDROID_TOOLCHAIN_BIN}:$ENV{PATH}\")\n"
            "get_filename_component(CROSSA_ANDROID_CMAKE_BIN \"${CMAKE_COMMAND}\" DIRECTORY)\n"
            "set(CROSSA_ANDROID_NINJA \"${CROSSA_ANDROID_CMAKE_BIN}/ninja\")\n"
            "if(NOT EXISTS \"${CROSSA_ANDROID_NINJA}\")\n"
            "    message(FATAL_ERROR \"The SDK CMake Ninja executable is required for Android curl provisioning.\")\n"
            "endif()\n"
            "set(CROSSA_ANDROID_OPENSSL_STAGE \"${CROSSA_ANDROID_THIRD_PARTY_DIRECTORY}/openssl-stage\")\n"
            "set(CROSSA_ANDROID_OPENSSL_INSTALL \"${CROSSA_ANDROID_OPENSSL_STAGE}/crossa\")\n\n"
            "ExternalProject_Add(crossa_android_openssl\n"
            "    URL \"" + AndroidBuildRequirements::openSslArchiveUrl() + "\"\n"
            "    URL_HASH \"SHA256=" +
                AndroidBuildRequirements::openSslArchiveSha256() + "\"\n"
            "    DOWNLOAD_DIR \"${CROSSA_ANDROID_DOWNLOAD_DIRECTORY}\"\n"
            "    SOURCE_DIR \"${CROSSA_ANDROID_THIRD_PARTY_DIRECTORY}/openssl-source\"\n"
            "    BINARY_DIR \"${CROSSA_ANDROID_THIRD_PARTY_DIRECTORY}/openssl-build\"\n"
            "    CONFIGURE_COMMAND ${CMAKE_COMMAND} -E env \"ANDROID_NDK_ROOT=${CMAKE_ANDROID_NDK}\" \"PATH=${CROSSA_ANDROID_TOOL_PATH}\" <SOURCE_DIR>/Configure android-arm64 -D__ANDROID_API__=23 no-shared no-tests --prefix=/crossa --openssldir=/crossa/ssl\n"
            "    BUILD_COMMAND ${CMAKE_COMMAND} -E env \"PATH=${CROSSA_ANDROID_TOOL_PATH}\" make -j4 build_libs\n"
            "    INSTALL_COMMAND ${CMAKE_COMMAND} -Dsource=<SOURCE_DIR> -Dbinary=<BINARY_DIR> -Ddestination=${CROSSA_ANDROID_OPENSSL_INSTALL} -P ${CMAKE_CURRENT_LIST_DIR}/CrossaOpenSslInstall.cmake\n"
            "    BUILD_BYPRODUCTS \"${CROSSA_ANDROID_OPENSSL_INSTALL}/lib/libssl.a\" \"${CROSSA_ANDROID_OPENSSL_INSTALL}/lib/libcrypto.a\"\n"
            ")\n\n"
            "set(CROSSA_ANDROID_CURL_INSTALL \"${CROSSA_ANDROID_THIRD_PARTY_DIRECTORY}/curl-install\")\n"
            "ExternalProject_Add(crossa_android_curl_build\n"
            "    DEPENDS crossa_android_openssl\n"
            "    URL \"" + AndroidBuildRequirements::curlArchiveUrl() + "\"\n"
            "    URL_HASH \"SHA256=" +
                AndroidBuildRequirements::curlArchiveSha256() + "\"\n"
            "    DOWNLOAD_DIR \"${CROSSA_ANDROID_DOWNLOAD_DIRECTORY}\"\n"
            "    SOURCE_DIR \"${CROSSA_ANDROID_THIRD_PARTY_DIRECTORY}/curl-source\"\n"
            "    BINARY_DIR \"${CROSSA_ANDROID_THIRD_PARTY_DIRECTORY}/curl-build\"\n"
            "    CMAKE_ARGS\n"
            "        -DCMAKE_TOOLCHAIN_FILE=${CMAKE_ANDROID_NDK}/build/cmake/android.toolchain.cmake\n"
            "        -DANDROID_ABI=${ANDROID_ABI}\n"
            "        -DANDROID_PLATFORM=android-23\n"
            "        -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}\n"
            "        -DCMAKE_DEBUG_POSTFIX=\n"
            "        -DCMAKE_MAKE_PROGRAM=${CROSSA_ANDROID_NINJA}\n"
            "        -DCMAKE_C_FLAGS=-ffile-prefix-map=${CROSSA_ANDROID_THIRD_PARTY_DIRECTORY}=/crossa-build\n"
            "        -DCMAKE_CXX_FLAGS=-ffile-prefix-map=${CROSSA_ANDROID_THIRD_PARTY_DIRECTORY}=/crossa-build\n"
            "        -DCMAKE_INSTALL_PREFIX=${CROSSA_ANDROID_CURL_INSTALL}\n"
            "        -DCMAKE_POSITION_INDEPENDENT_CODE=ON\n"
            "        -DBUILD_SHARED_LIBS=OFF\n"
            "        -DBUILD_CURL_EXE=OFF\n"
            "        -DBUILD_EXAMPLES=OFF\n"
            "        -DBUILD_TESTING=OFF\n"
            "        -DCURL_USE_OPENSSL=ON\n"
            "        -DOPENSSL_ROOT_DIR=${CROSSA_ANDROID_OPENSSL_INSTALL}\n"
            "        -DOPENSSL_USE_STATIC_LIBS=TRUE\n"
            "        -DOPENSSL_INCLUDE_DIR=${CROSSA_ANDROID_OPENSSL_INSTALL}/include\n"
            "        -DOPENSSL_SSL_LIBRARY=${CROSSA_ANDROID_OPENSSL_INSTALL}/lib/libssl.a\n"
            "        -DOPENSSL_CRYPTO_LIBRARY=${CROSSA_ANDROID_OPENSSL_INSTALL}/lib/libcrypto.a\n"
            "        -DCURL_USE_LIBPSL=OFF\n"
            "        -DCURL_BROTLI=OFF\n"
            "        -DCURL_ZSTD=OFF\n"
            "        -DUSE_NGHTTP2=OFF\n"
            "        -DCURL_USE_GSSAPI=OFF\n"
            "        -DCURL_DISABLE_LDAP=ON\n"
            "        -DCURL_DISABLE_LDAPS=ON\n"
            "        -DCURL_DISABLE_RTSP=ON\n"
            "        -DCURL_DISABLE_DICT=ON\n"
            "        -DCURL_DISABLE_TELNET=ON\n"
            "        -DCURL_DISABLE_TFTP=ON\n"
            "        -DCURL_DISABLE_POP3=ON\n"
            "        -DCURL_DISABLE_IMAP=ON\n"
            "        -DCURL_DISABLE_SMTP=ON\n"
            "        -DCURL_DISABLE_GOPHER=ON\n"
            "        -DCURL_DISABLE_MQTT=ON\n"
            "        -DCURL_CA_BUNDLE=none\n"
            "        -DCURL_CA_PATH=none\n"
            "    BUILD_BYPRODUCTS \"${CROSSA_ANDROID_CURL_INSTALL}/lib/libcurl.a\"\n"
            ")\n\n"
            "add_library(crossa_android_curl STATIC IMPORTED GLOBAL)\n"
            "set_target_properties(crossa_android_curl PROPERTIES IMPORTED_LOCATION \"${CROSSA_ANDROID_CURL_INSTALL}/lib/libcurl.a\")\n"
            "add_dependencies(crossa_android_curl crossa_android_curl_build)\n"
            "add_library(crossa_android_ssl STATIC IMPORTED GLOBAL)\n"
            "set_target_properties(crossa_android_ssl PROPERTIES IMPORTED_LOCATION \"${CROSSA_ANDROID_OPENSSL_INSTALL}/lib/libssl.a\")\n"
            "add_dependencies(crossa_android_ssl crossa_android_openssl)\n"
            "add_library(crossa_android_crypto STATIC IMPORTED GLOBAL)\n"
            "set_target_properties(crossa_android_crypto PROPERTIES IMPORTED_LOCATION \"${CROSSA_ANDROID_OPENSSL_INSTALL}/lib/libcrypto.a\")\n"
            "add_dependencies(crossa_android_crypto crossa_android_openssl)\n"
        );
    }

    // Writes the Gradle project and Android library build definitions.
    void AndroidProjectGenerator::writeBuildFiles(
        const filesystem::path& outputDirectory,
        const string& packageName
    ) {
        writeGradleWrapper(outputDirectory);
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
            "        ndk { abiFilters += listOf(\"" +
                AndroidBuildRequirements::supportedAbi() + "\") }\n"
            "        consumerProguardFiles(\"consumer-rules.pro\")\n"
            "        externalNativeBuild { cmake { cppFlags += listOf(\"-std=c++20\", \"-O3\") } }\n"
            "    }\n\n"
            "    buildTypes {\n"
            "        release { isMinifyEnabled = false }\n"
            "    }\n\n"
            "    externalNativeBuild { cmake { path = file(\"src/main/cpp/CMakeLists.txt\") } }\n"
            "}\n\n"
            "kotlin { jvmToolchain(" +
                to_string(AndroidBuildRequirements::javaToolchainVersion()) +
                ") }\n"
        );
        writeFile(
            outputDirectory / "library" / "consumer-rules.pro",
            "-keep,allowoptimization class " + packageName +
                ".CrossaNativeBridge { *; }\n"
            "-keep,allowoptimization class " + packageName +
                ".CrossaNativeCallback { *; }\n"
            "-keep,allowoptimization class " + packageName +
                ".CrossaArgument { *; }\n"
            "-keep,allowoptimization class " + packageName +
                ".CrossaArgument$IntValue { *; }\n"
            "-keep,allowoptimization class " + packageName +
                ".CrossaArgument$LongValue { *; }\n"
            "-keep,allowoptimization class " + packageName +
                ".CrossaArgument$DoubleValue { *; }\n"
            "-keep,allowoptimization class " + packageName +
                ".CrossaArgument$StringValue { *; }\n"
            "-keep,allowoptimization class " + packageName +
                ".CrossaArgument$BooleanValue { *; }\n"
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
            "    private val runtime: Long,\n"
            "    private var handle: Long\n"
            ") : AutoCloseable {\n"
            "    internal fun requireHandle(): Long = check(handle != 0L) { \"Crossa native result is closed.\" }.let { handle }\n"
            "    internal fun rootModelHandle(): Long = CrossaNativeBridge.rootModelHandle(this)\n"
            "    internal fun intValue(): Int = CrossaNativeBridge.resultInt(this)\n"
            "    internal fun longValue(): Long = CrossaNativeBridge.resultLong(this)\n"
            "    internal fun doubleValue(): Double = CrossaNativeBridge.resultDouble(this)\n"
            "    internal fun stringValue(): String = CrossaNativeBridge.resultString(this)\n"
            "    internal fun booleanValue(): Boolean = CrossaNativeBridge.resultBoolean(this)\n"
            "    internal fun runtimeHandle(): Long = runtime\n"
            "    override fun close() { if (handle != 0L) { CrossaNativeBridge.releaseResult(runtime, handle); handle = 0L } }\n"
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
            "internal fun interface CrossaNativeCallback {\n"
            "    fun onComplete(state: Int, valueHandle: Long, errorHandle: Long)\n"
            "}\n\n"
            "internal object CrossaNativeBridge {\n"
            "    init { System.loadLibrary(\"crossa_runtime\") }\n"
            "    internal fun configure(): Long = nativeConfigure()\n"
            "    internal fun invokeAsync(runtime: Long, operationId: Long, arguments: Array<CrossaArgument>): Long = nativeInvokeAsync(runtime, operationId, arguments)\n"
            "    internal fun <T> invokeAsyncAfter(runtime: Long, operationId: Long, arguments: Array<CrossaArgument>, mapper: (CrossaNativeResult) -> T, onState: (CrossaState<T>) -> Unit): Long {\n"
            "        return nativeInvokeAsyncAfter(runtime, operationId, arguments, CrossaNativeCallback { state, valueHandle, errorHandle ->\n"
            "            when (state) {\n"
            "                0 -> onState(CrossaState.Success(mapper(CrossaNativeResult(runtime, valueHandle))))\n"
            "                1 -> {\n"
            "                    val message = nativeErrorMessage(runtime, errorHandle)\n"
            "                    nativeReleaseError(runtime, errorHandle)\n"
            "                    onState(CrossaState.Failed(CrossaError(0, 0, message, false)))\n"
            "                }\n"
            "                else -> onState(CrossaState.Cancelled)\n"
            "            }\n"
            "        })\n"
            "    }\n"
            "    internal fun releaseResult(runtime: Long, handle: Long) { nativeReleaseResult(runtime, handle) }\n"
            "    internal fun listSize(result: CrossaNativeResult): Int = nativeListSize(result.runtimeHandle(), result.requireHandle())\n"
            "    internal fun listModelHandle(result: CrossaNativeResult, index: Int): Long = nativeListModelHandle(result.runtimeHandle(), result.requireHandle(), index)\n"
            "    internal fun rootModelHandle(result: CrossaNativeResult): Long = nativeRootModelHandle(result.runtimeHandle(), result.requireHandle())\n"
            "    internal fun resultInt(result: CrossaNativeResult): Int = nativeResultInt(result.runtimeHandle(), result.requireHandle())\n"
            "    internal fun resultLong(result: CrossaNativeResult): Long = nativeResultLong(result.runtimeHandle(), result.requireHandle())\n"
            "    internal fun resultDouble(result: CrossaNativeResult): Double = nativeResultDouble(result.runtimeHandle(), result.requireHandle())\n"
            "    internal fun resultString(result: CrossaNativeResult): String = nativeResultString(result.runtimeHandle(), result.requireHandle())\n"
            "    internal fun resultBoolean(result: CrossaNativeResult): Boolean = nativeResultBoolean(result.runtimeHandle(), result.requireHandle())\n"
            "    internal fun modelInt(result: CrossaNativeResult, model: Long, field: Int): Int = nativeModelInt(result.runtimeHandle(), result.requireHandle(), model, field)\n"
            "    internal fun modelLong(result: CrossaNativeResult, model: Long, field: Int): Long = nativeModelLong(result.runtimeHandle(), result.requireHandle(), model, field)\n"
            "    internal fun modelDouble(result: CrossaNativeResult, model: Long, field: Int): Double = nativeModelDouble(result.runtimeHandle(), result.requireHandle(), model, field)\n"
            "    internal fun modelString(result: CrossaNativeResult, model: Long, field: Int): String = nativeModelString(result.runtimeHandle(), result.requireHandle(), model, field)\n"
            "    internal fun modelBoolean(result: CrossaNativeResult, model: Long, field: Int): Boolean = nativeModelBoolean(result.runtimeHandle(), result.requireHandle(), model, field)\n"
            "    internal fun cancel(runtime: Long, operation: Long): Boolean = nativeCancel(runtime, operation)\n"
            "    internal fun releaseOperation(runtime: Long, operation: Long) { nativeReleaseOperation(runtime, operation) }\n"
            "    internal fun shutdown(runtime: Long) { nativeShutdown(runtime) }\n"
            "    internal fun releaseRuntime(runtime: Long) { nativeReleaseRuntime(runtime) }\n"
            "    private external fun nativeConfigure(): Long\n"
            "    private external fun nativeInvokeAsync(runtime: Long, operationId: Long, arguments: Array<CrossaArgument>): Long\n"
            "    private external fun nativeInvokeAsyncAfter(runtime: Long, operationId: Long, arguments: Array<CrossaArgument>, callback: CrossaNativeCallback): Long\n"
            "    private external fun nativeCancel(runtime: Long, operation: Long): Boolean\n"
            "    private external fun nativeReleaseOperation(runtime: Long, operation: Long)\n"
            "    private external fun nativeShutdown(runtime: Long)\n"
            "    private external fun nativeReleaseRuntime(runtime: Long)\n"
            "    private external fun nativeReleaseResult(runtime: Long, result: Long)\n"
            "    private external fun nativeListSize(runtime: Long, result: Long): Int\n"
            "    private external fun nativeListModelHandle(runtime: Long, result: Long, index: Int): Long\n"
            "    private external fun nativeRootModelHandle(runtime: Long, result: Long): Long\n"
            "    private external fun nativeResultInt(runtime: Long, result: Long): Int\n"
            "    private external fun nativeResultLong(runtime: Long, result: Long): Long\n"
            "    private external fun nativeResultDouble(runtime: Long, result: Long): Double\n"
            "    private external fun nativeResultString(runtime: Long, result: Long): String\n"
            "    private external fun nativeErrorMessage(runtime: Long, error: Long): String\n"
            "    private external fun nativeReleaseError(runtime: Long, error: Long)\n"
            "    private external fun nativeResultBoolean(runtime: Long, result: Long): Boolean\n"
            "    private external fun nativeModelInt(runtime: Long, result: Long, model: Long, field: Int): Int\n"
            "    private external fun nativeModelLong(runtime: Long, result: Long, model: Long, field: Int): Long\n"
            "    private external fun nativeModelDouble(runtime: Long, result: Long, model: Long, field: Int): Double\n"
            "    private external fun nativeModelString(runtime: Long, result: Long, model: Long, field: Int): String\n"
            "    private external fun nativeModelBoolean(runtime: Long, result: Long, model: Long, field: Int): Boolean\n"
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
            "    private var handle: Long = 0L\n\n"
            "    public fun configure(overrides: CrossaConfigurationOverrides) {\n"
            "        require(handle == 0L) { \"Crossa runtime is already configured.\" }\n"
            "        handle = CrossaNativeBridge.configure()\n"
            "        check(handle != 0L) { \"Crossa native runtime creation failed.\" }\n"
            "    }\n\n"
            "    internal fun requireHandle(): Long = check(handle != 0L) { \"Crossa runtime is not configured.\" }.let { handle }\n\n"
            "    public fun shutdown() { if (handle != 0L) CrossaNativeBridge.shutdown(handle) }\n"
            "    public fun close() { if (handle != 0L) { CrossaNativeBridge.releaseRuntime(handle); handle = 0L } }\n"
            "}\n"
        );
    }

    // Writes the Android manifest and native CMake project source.
    void AndroidProjectGenerator::writeNativeBuildFiles(
        const filesystem::path& outputDirectory,
        const string& packageName,
        const vector<const compiler::ir::Program*>& programs
    ) {
        const filesystem::path nativeDirectory = outputDirectory / "library" /
            "src" / "main" / "cpp";
#ifdef CROSSA_SOURCE_DIRECTORY
        const filesystem::path sourceRoot(CROSSA_SOURCE_DIRECTORY);
        error_code copyError;
        filesystem::create_directories(nativeDirectory / "crossa" / "include", copyError);
        if (copyError) throw runtime_error("Unable to create embedded Crossa header directory.");
        filesystem::copy(
            sourceRoot / "include" / "crossa",
            nativeDirectory / "crossa" / "include" / "crossa",
            filesystem::copy_options::recursive,
            copyError
        );
        if (copyError) throw runtime_error("Unable to embed Crossa runtime headers.");
        const vector<filesystem::path> runtimeDirectories = {
            "src/runtime", "src/network", "src/bindings/shared-abi",
            "src/bindings/android", "src/compiler/ir", "src/utils"
        };
        for (const filesystem::path& directory : runtimeDirectories) {
            filesystem::create_directories(
                (nativeDirectory / "crossa" / directory).parent_path(),
                copyError
            );
            if (copyError) throw runtime_error("Unable to create embedded Crossa source directory.");
            filesystem::copy(
                sourceRoot / directory,
                nativeDirectory / "crossa" / directory,
                filesystem::copy_options::recursive,
                copyError
            );
            if (copyError) throw runtime_error("Unable to embed Crossa native runtime source.");
        }
        for (const filesystem::path& source : {filesystem::path("src/compiler/source/SourceLocation.cpp"), filesystem::path("src/compiler/types/SemanticType.cpp")}) {
            filesystem::create_directories((nativeDirectory / "crossa" / source).parent_path(), copyError);
            filesystem::copy_file(sourceRoot / source, nativeDirectory / "crossa" / source, filesystem::copy_options::overwrite_existing, copyError);
            if (copyError) throw runtime_error("Unable to embed Crossa native compiler support.");
        }
        filesystem::remove(
            nativeDirectory / "crossa" / "src" / "bindings" / "android" /
                "AndroidUnavailableCurlTransport.cpp",
            copyError
        );
        if (copyError) throw runtime_error("Unable to remove the Android network fallback source.");
#else
        throw runtime_error("Crossa Android generation requires an embedded runtime source directory.");
#endif
        writeFile(
            outputDirectory / "library" / "src" / "main" / "AndroidManifest.xml",
            "<manifest xmlns:android=\"http://schemas.android.com/apk/res/android\" />\n"
        );
        writeAndroidDependencies(outputDirectory);
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
            "file(GLOB_RECURSE CROSSA_RUNTIME_SOURCES CONFIGURE_DEPENDS\n"
            "    \"${CMAKE_CURRENT_LIST_DIR}/crossa/src/runtime/*.cpp\"\n"
            "    \"${CMAKE_CURRENT_LIST_DIR}/crossa/src/network/*.cpp\"\n"
            "    \"${CMAKE_CURRENT_LIST_DIR}/crossa/src/bindings/shared-abi/*.cpp\"\n"
            "    \"${CMAKE_CURRENT_LIST_DIR}/crossa/src/bindings/android/*.cpp\"\n"
            "    \"${CMAKE_CURRENT_LIST_DIR}/crossa/src/compiler/ir/*.cpp\"\n"
            "    \"${CMAKE_CURRENT_LIST_DIR}/crossa/src/compiler/source/SourceLocation.cpp\"\n"
            "    \"${CMAKE_CURRENT_LIST_DIR}/crossa/src/compiler/types/SemanticType.cpp\"\n"
            "    \"${CMAKE_CURRENT_LIST_DIR}/crossa/src/utils/*.cpp\"\n"
            ")\n"
            "list(FILTER CROSSA_RUNTIME_SOURCES EXCLUDE REGEX \"bindings/android/AndroidUnavailableCurlTransport.cpp$\")\n"
            "list(FILTER CROSSA_RUNTIME_SOURCES EXCLUDE REGEX \"compiler/ir/IrLowerer.cpp$\")\n"
            "list(FILTER CROSSA_RUNTIME_SOURCES EXCLUDE REGEX \"compiler/ir/IrPrinter.cpp$\")\n"
            "include(${CMAKE_CURRENT_LIST_DIR}/CrossaAndroidDependencies.cmake)\n"
            "add_library(crossa_runtime SHARED crossa_runtime.cpp CrossaGeneratedProgram.cpp ${CROSSA_RUNTIME_SOURCES})\n"
            "target_compile_features(crossa_runtime PRIVATE cxx_std_20)\n"
            "target_include_directories(crossa_runtime PRIVATE ${CMAKE_CURRENT_LIST_DIR} ${CMAKE_CURRENT_BINARY_DIR} ${CMAKE_CURRENT_LIST_DIR}/crossa/include ${CROSSA_ANDROID_OPENSSL_INSTALL}/include ${CROSSA_ANDROID_CURL_INSTALL}/include)\n"
            "target_compile_definitions(crossa_runtime PRIVATE CROSSA_ANDROID_EMBEDDED_CA_BUNDLE=1)\n"
            "target_compile_options(crossa_runtime PRIVATE -O3 -fvisibility=hidden -fvisibility-inlines-hidden -ffile-prefix-map=${CMAKE_CURRENT_LIST_DIR}=/crossa-source)\n"
            "target_link_options(crossa_runtime PRIVATE -Wl,-z,max-page-size=16384 -Wl,-z,common-page-size=16384)\n"
            "target_link_libraries(crossa_runtime PRIVATE crossa_android_curl crossa_android_ssl crossa_android_crypto android log z)\n"
            "add_dependencies(crossa_runtime crossa_android_curl_build)\n"
        );
        const string packagePathValue = [&packageName]() {
            string result;
            for (const char value : packageName) {
                result += value == '.' ? '/' : value;
            }
            return result;
        }();
        writeFile(
            outputDirectory / "library" / "src" / "main" / "cpp" / "crossa_runtime.cpp",
            "#include <jni.h>\n\n"
            "#include <utility>\n\n"
            "#include \"CrossaGeneratedProgram.h\"\n"
            "#include <crossa/bindings/android/AndroidJniBridge.h>\n"
            "#include <crossa/bindings/shared-abi/CrossaAbiRuntimeFactory.h>\n"
            "#include <crossa/utils/Log.h>\n\n"
            "namespace {\n\n"
            "CrossaRuntimeHandle createGeneratedRuntime() {\n"
            "    static const crossa::utils::Log log(crossa::utils::Log::Level::Error);\n"
            "    CrossaRuntimeHandle runtime = 0;\n"
            "    crossa::compiler::ir::Program program = crossa::generated::CrossaGeneratedProgram::create();\n"
            "    const CrossaStatus status = crossa::bindings::sharedabi::CrossaAbiRuntimeFactory::create(\n"
            "        std::move(program),\n"
            "        nullptr,\n"
            "        log,\n"
            "        &runtime\n"
            "    );\n"
            "    return status == CrossaStatusOk ? runtime : 0;\n"
            "}\n\n"
            "}\n\n"
            "extern \"C\" JNIEXPORT jint JNI_OnLoad(JavaVM* javaVm, void*) {\n"
            "    JNIEnv* environment = nullptr;\n"
            "    if (javaVm->GetEnv(reinterpret_cast<void**>(&environment), JNI_VERSION_1_6) != JNI_OK) {\n"
            "        return JNI_ERR;\n"
            "    }\n"
            "    return crossa::bindings::android::AndroidJniBridge::initialize(\n"
            "        javaVm,\n"
            "        environment,\n"
            "        \"" + packagePathValue + "_CrossaNativeBridge\",\n"
            "        createGeneratedRuntime\n"
            "    ) ? JNI_VERSION_1_6 : JNI_ERR;\n"
            "}\n"
        );
    }

}
