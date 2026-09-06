#include "crossa/packaging/android/AndroidProjectGenerator.h"

#include <algorithm>
#include <fstream>
#include <iterator>
#include <set>
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
        const filesystem::path& outputDirectory,
        const AndroidBuildVersions& buildVersions
    ) const {
        const string resolvedPackageName = resolvePackageName(packageName);
        createDirectory(outputDirectory);
        writeBuildFiles(outputDirectory, resolvedPackageName, buildVersions);
        writeNativeBuildFiles(outputDirectory, resolvedPackageName, programs);

        const filesystem::path kotlinDirectory = outputDirectory / "library" /
            "src" / "main" / "kotlin" / packagePath(resolvedPackageName);
        createDirectory(kotlinDirectory);
        vector<string> generatedPaths = {
            "runtime/CrossaState.kt",
            "runtime/CrossaError.kt",
            "runtime/CrossaNativeResult.kt",
            "runtime/CrossaNativeValue.kt",
            "runtime/CrossaNativeList.kt",
            "runtime/CrossaJson.kt",
            "runtime/CrossaConfigurationOverrides.kt",
            "runtime/CrossaOperation.kt",
            "runtime/CrossaRuntime.kt",
            "internal/CrossaArgument.kt",
            "internal/CrossaNativeBridge.kt"
        };
        for (const auto& source : kotlinSources) {
            generatedPaths.push_back(source.getFileName());
        }
        removeStaleKotlinSources(kotlinDirectory, generatedPaths);
        writeRuntimeApi(kotlinDirectory, resolvedPackageName);
        for (const auto& source : kotlinSources) {
            writeFile(kotlinDirectory / source.getFileName(), source.getContent());
        }
        writeKotlinManifest(kotlinDirectory, generatedPaths);
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
        error_code error;
        if (filesystem::exists(outputPath, error) && !error) {
            ifstream existing(outputPath, ios::binary);
            const string existingContent(
                (istreambuf_iterator<char>(existing)),
                istreambuf_iterator<char>()
            );
            if (existing && existingContent == content) {
                return;
            }
        }
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
        error.clear();
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

    // Removes outputs listed by the previous generated Kotlin manifest only.
    void AndroidProjectGenerator::removeStaleKotlinSources(
        const filesystem::path& kotlinDirectory,
        const vector<string>& currentPaths
    ) {
        const filesystem::path manifestPath =
            kotlinDirectory / ".crossa-generated-kotlin-manifest";
        ifstream manifest(manifestPath);
        if (!manifest.is_open()) {
            return;
        }
        const set<string> currentPathSet(currentPaths.begin(), currentPaths.end());
        string pathValue;
        while (getline(manifest, pathValue)) {
            const filesystem::path relativePath(pathValue);
            if (pathValue.empty() || relativePath.is_absolute() ||
                pathValue.find("..") != string::npos ||
                currentPathSet.find(pathValue) != currentPathSet.end()) {
                continue;
            }
            error_code error;
            filesystem::remove(kotlinDirectory / relativePath, error);
            if (error) {
                throw runtime_error(
                    "Unable to remove stale generated Kotlin source: " +
                    (kotlinDirectory / relativePath).string()
                );
            }
        }
    }

    // Writes the stable manifest used to remove stale Kotlin source outputs.
    void AndroidProjectGenerator::writeKotlinManifest(
        const filesystem::path& kotlinDirectory,
        const vector<string>& currentPaths
    ) {
        vector<string> sortedPaths = currentPaths;
        sort(sortedPaths.begin(), sortedPaths.end());
        string manifest;
        for (const string& path : sortedPaths) {
            manifest += path + "\n";
        }
        writeFile(kotlinDirectory / ".crossa-generated-kotlin-manifest", manifest);
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

    // Copies the trusted Gradle Wrapper and selects its requested distribution version.
    void AndroidProjectGenerator::writeGradleWrapper(
        const filesystem::path& outputDirectory,
        const string& gradleVersion
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
        writeFile(
            outputDirectory / "gradle" / "wrapper" /
                "gradle-wrapper.properties",
            "distributionBase=GRADLE_USER_HOME\n"
            "distributionPath=wrapper/dists\n"
            "distributionUrl=https\\://services.gradle.org/distributions/gradle-" +
                gradleVersion + "-bin.zip\n"
            "networkTimeout=10000\n"
            "validateDistributionUrl=true\n"
            "zipStoreBase=GRADLE_USER_HOME\n"
            "zipStorePath=wrapper/dists\n"
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
        const string& packageName,
        const AndroidBuildVersions& buildVersions
    ) {
        writeGradleWrapper(outputDirectory, buildVersions.gradleVersion);
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
                buildVersions.kotlinVersion +
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
                buildVersions.ndkVersion +
                "\"\n\n"
            "    defaultConfig {\n"
            "        minSdk = 23\n"
            "        ndk { abiFilters += listOf(\"" +
                AndroidBuildRequirements::supportedAbi() + "\") }\n"
            "        consumerProguardFiles(\"consumer-rules.pro\")\n"
            "        externalNativeBuild { cmake { cppFlags += listOf(\"-std=c++20\") } }\n"
            "    }\n\n"
            "    buildTypes {\n"
            "        debug {\n"
            "            isJniDebuggable = true\n"
            "            isMinifyEnabled = false\n"
            "        }\n"
            "        release {\n"
            "            isJniDebuggable = false\n"
            "            isMinifyEnabled = true\n"
            "            proguardFiles(getDefaultProguardFile(\"proguard-android-optimize.txt\"), \"proguard-rules.pro\")\n"
            "        }\n"
            "    }\n\n"
            "    externalNativeBuild { cmake { path = file(\"src/main/cpp/CMakeLists.txt\") } }\n"
            "}\n\n"
            "kotlin { jvmToolchain(" +
                to_string(AndroidBuildRequirements::javaToolchainVersion()) +
                ") }\n\n"
            "dependencies { implementation(\"org.jetbrains.kotlinx:kotlinx-coroutines-core:1.10.2\") }\n"
        );
        writeFile(
            outputDirectory / "library" / "consumer-rules.pro",
            "-keep,allowoptimization public class " + packageName +
                ".api.** { public *; }\n"
            "-keep,allowoptimization public class " + packageName +
                ".model.** { public *; }\n"
            "-keep,allowoptimization public class " + packageName +
                ".runtime.CrossaRuntime { public *; }\n"
            "-keep,allowoptimization public class " + packageName +
                ".runtime.CrossaState { public *; }\n"
            "-keep,allowoptimization public class " + packageName +
                ".runtime.CrossaOperation { public *; }\n"
            "-keep,allowoptimization public class " + packageName +
                ".runtime.CrossaNativeList { public *; }\n"
            "-keep,allowoptimization public class " + packageName +
                ".runtime.CrossaJson { public *; }\n"
            "-keep class " + packageName +
                ".internal.CrossaNativeBridge { <methods>; }\n"
            "-keep class " + packageName +
                ".internal.CrossaNativeCallback { <methods>; }\n"
            "-keep class " + packageName +
                ".internal.CrossaArgument { *; }\n"
            "-keep class " + packageName +
                ".internal.CrossaArgument$IntValue { *; }\n"
            "-keep class " + packageName +
                ".internal.CrossaArgument$LongValue { *; }\n"
            "-keep class " + packageName +
                ".internal.CrossaArgument$DoubleValue { *; }\n"
            "-keep class " + packageName +
                ".internal.CrossaArgument$StringValue { *; }\n"
            "-keep class " + packageName +
                ".internal.CrossaArgument$BooleanValue { *; }\n"
            "-dontwarn java.lang.invoke.StringConcatFactory\n"
        );
        writeFile(
            outputDirectory / "library" / "proguard-rules.pro",
            "-keep,allowoptimization public class " + packageName +
                ".api.** { public *; }\n"
            "-keep,allowoptimization public class " + packageName +
                ".model.** { public *; }\n"
            "-keep,allowoptimization public class " + packageName +
                ".runtime.CrossaRuntime { public *; }\n"
            "-keep,allowoptimization public class " + packageName +
                ".runtime.CrossaState { public *; }\n"
            "-keep,allowoptimization public class " + packageName +
                ".runtime.CrossaOperation { public *; }\n"
            "-keep,allowoptimization public class " + packageName +
                ".runtime.CrossaNativeList { public *; }\n"
            "-keep,allowoptimization public class " + packageName +
                ".runtime.CrossaJson { public *; }\n"
            "-keep class " + packageName +
                ".internal.CrossaNativeBridge { <methods>; }\n"
            "-dontwarn java.lang.invoke.StringConcatFactory\n"
        );
    }

    // Writes the generated configuration API and JNI bridge declarations.
    void AndroidProjectGenerator::writeRuntimeApi(
        const filesystem::path& sourceDirectory,
        const string& packageName
    ) {
        writeFile(
            sourceDirectory / "runtime" / "CrossaState.kt",
            "package " + packageName + ".runtime\n\n"
            "public sealed interface CrossaState<out T> {\n"
            "    public data class Success<T>(public val data: T) : CrossaState<T>\n"
            "    public data class Failed(public val error: CrossaError) : CrossaState<Nothing>\n"
            "    public data object Cancelled : CrossaState<Nothing>\n"
            "}\n"
        );
        writeFile(
            sourceDirectory / "runtime" / "CrossaError.kt",
            "package " + packageName + ".runtime\n\n"
            "public data class CrossaError(\n"
            "    public val domain: Int,\n"
            "    public val code: Int,\n"
            "    override val message: String,\n"
            "    public val retryable: Boolean\n"
            ") : Exception(message)\n"
        );
        writeFile(
            sourceDirectory / "runtime" / "CrossaNativeResult.kt",
            "package " + packageName + ".runtime\n\n"
            "import " + packageName + ".internal.CrossaNativeBridge\n\n"
            "public class CrossaNativeResult internal constructor(\n"
            "    private val runtime: Long,\n"
            "    private var handle: Long\n"
            ") : AutoCloseable {\n"
            "    @Synchronized internal fun requireHandle(): Long = check(handle != 0L) { \"Crossa native result is closed.\" }.let { handle }\n"
            "    internal fun runtimeHandle(): Long = runtime\n"
            "    internal fun rootValue(): CrossaNativeValue = CrossaNativeValue(this, CrossaNativePath.root())\n"
            "    internal fun intValue(): Int = rootValue().intValue().also { close() }\n"
            "    internal fun longValue(): Long = rootValue().longValue().also { close() }\n"
            "    internal fun doubleValue(): Double = rootValue().doubleValue().also { close() }\n"
            "    internal fun stringValue(): String = rootValue().stringValue().also { close() }\n"
            "    internal fun booleanValue(): Boolean = rootValue().booleanValue().also { close() }\n"
            "    @Synchronized override fun close() { if (handle != 0L) { CrossaNativeBridge.releaseResult(runtime, handle); handle = 0L } }\n"
            "}\n"
        );
        writeFile(
            sourceDirectory / "runtime" / "CrossaNativeValue.kt",
            "package " + packageName + ".runtime\n\n"
            "import " + packageName + ".internal.CrossaNativeBridge\n\n"
            "internal class CrossaNativePath private constructor(\n"
            "    private val packed: IntArray\n"
            ") {\n"
            "    fun appendField(index: Int): CrossaNativePath {\n"
            "        require(index >= 0) { \"Crossa field index must be non-negative.\" }\n"
            "        return CrossaNativePath(packed + intArrayOf(FIELD, index))\n"
            "    }\n"
            "    fun appendElement(index: Int): CrossaNativePath {\n"
            "        require(index >= 0) { \"Crossa list index must be non-negative.\" }\n"
            "        return CrossaNativePath(packed + intArrayOf(ELEMENT, index))\n"
            "    }\n"
            "    fun toPackedArray(): IntArray = packed\n"
            "    companion object {\n"
            "        const val FIELD = 0\n"
            "        const val ELEMENT = 1\n"
            "        fun root(): CrossaNativePath = CrossaNativePath(IntArray(0))\n"
            "    }\n"
            "}\n\n"
            "public class CrossaNativeValue internal constructor(\n"
            "    private val owner: CrossaNativeResult,\n"
            "    private val path: CrossaNativePath\n"
            ") {\n"
            "    public fun child(fieldIndex: Int): CrossaNativeValue =\n"
            "        CrossaNativeValue(owner, path.appendField(fieldIndex))\n"
            "    public fun element(index: Int): CrossaNativeValue =\n"
            "        CrossaNativeValue(owner, path.appendElement(index))\n"
            "    public fun intValue(): Int = CrossaNativeBridge.valueInt(owner, path)\n"
            "    public fun longValue(): Long = CrossaNativeBridge.valueLong(owner, path)\n"
            "    public fun doubleValue(): Double = CrossaNativeBridge.valueDouble(owner, path)\n"
            "    public fun stringValue(): String = CrossaNativeBridge.valueString(owner, path)\n"
            "    public fun booleanValue(): Boolean = CrossaNativeBridge.valueBoolean(owner, path)\n"
            "    public fun listSize(): Int = CrossaNativeBridge.valueListSize(owner, path)\n"
            "    internal fun jsonKind(): Int = CrossaNativeBridge.valueJsonKind(owner, path)\n"
            "    internal fun jsonBoolean(): Boolean = CrossaNativeBridge.valueJsonBoolean(owner, path)\n"
            "    internal fun jsonNumberText(): String = CrossaNativeBridge.valueJsonNumber(owner, path)\n"
            "    internal fun jsonString(): String = CrossaNativeBridge.valueJsonString(owner, path)\n"
            "    internal fun jsonSize(): Int = CrossaNativeBridge.valueJsonSize(owner, path)\n"
            "    internal fun jsonKey(index: Int): String = CrossaNativeBridge.valueJsonKey(owner, path, index)\n"
            "    internal fun closeOwner() { owner.close() }\n"
            "}\n"
        );
        writeFile(
            sourceDirectory / "runtime" / "CrossaNativeList.kt",
            "package " + packageName + ".runtime\n\n"
            "public class CrossaNativeList<T> internal constructor(\n"
            "    private val nativeValue: CrossaNativeValue,\n"
            "    private val mapper: (CrossaNativeValue) -> T\n"
            ") : AbstractList<T>(), AutoCloseable {\n"
            "    override val size: Int get() = nativeValue.listSize()\n"
            "    override fun get(index: Int): T {\n"
            "        if (index < 0 || index >= size) throw IndexOutOfBoundsException(\"index \" + index.toString())\n"
            "        return mapper(nativeValue.element(index))\n"
            "    }\n"
            "    override fun close() { nativeValue.closeOwner() }\n"
            "}\n"
        );
        writeFile(
            sourceDirectory / "runtime" / "CrossaJson.kt",
            "package " + packageName + ".runtime\n\n"
            "public enum class CrossaJsonKind {\n"
            "    Null, Boolean, Number, String, Array, Object\n"
            "}\n\n"
            "public class CrossaJson internal constructor(\n"
            "    private val nativeValue: CrossaNativeValue\n"
            ") : AutoCloseable {\n"
            "    public val kind: CrossaJsonKind\n"
            "        get() = when (nativeValue.jsonKind()) {\n"
            "            1 -> CrossaJsonKind.Boolean\n"
            "            2 -> CrossaJsonKind.Number\n"
            "            3 -> CrossaJsonKind.String\n"
            "            4 -> CrossaJsonKind.Array\n"
            "            5 -> CrossaJsonKind.Object\n"
            "            else -> CrossaJsonKind.Null\n"
            "        }\n"
            "    public val size: Int get() = nativeValue.jsonSize()\n"
            "    public fun booleanValue(): Boolean = nativeValue.jsonBoolean()\n"
            "    public fun numberText(): String = nativeValue.jsonNumberText()\n"
            "    public fun stringValue(): String = nativeValue.jsonString()\n"
            "    public fun key(index: Int): String = nativeValue.jsonKey(index)\n"
            "    public fun field(index: Int): CrossaJson = CrossaJson(nativeValue.child(index))\n"
            "    public fun field(name: String): CrossaJson {\n"
            "        for (index in 0 until size) {\n"
            "            if (key(index) == name) return field(index)\n"
            "        }\n"
            "        throw NoSuchElementException(name)\n"
            "    }\n"
            "    public fun element(index: Int): CrossaJson = CrossaJson(nativeValue.element(index))\n"
            "    override fun close() { nativeValue.closeOwner() }\n"
            "}\n"
        );
        writeFile(
            sourceDirectory / "runtime" / "CrossaOperation.kt",
            "package " + packageName + ".runtime\n\n"
            "import " + packageName + ".internal.CrossaNativeBridge\n\n"
            "public class CrossaOperation internal constructor(\n"
            "    private val runtime: Long,\n"
            "    private var handle: Long\n"
            ") : AutoCloseable {\n"
            "    @Synchronized public fun cancel() { if (handle != 0L) CrossaNativeBridge.cancel(runtime, handle) }\n"
            "    @Synchronized override fun close() { if (handle != 0L) { CrossaNativeBridge.releaseOperation(runtime, handle); handle = 0L } }\n"
            "}\n"
        );
        writeFile(
            sourceDirectory / "internal" / "CrossaArgument.kt",
            "package " + packageName + ".internal\n\n"
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
            sourceDirectory / "internal" / "CrossaNativeBridge.kt",
            "package " + packageName + ".internal\n\n"
            "import " + packageName + ".runtime.CrossaError\n"
            "import " + packageName + ".runtime.CrossaNativePath\n"
            "import " + packageName + ".runtime.CrossaNativeResult\n"
            "import " + packageName + ".runtime.CrossaState\n\n"
            "import " + packageName + ".runtime.CrossaConfigurationOverrides\n"
            "import kotlinx.coroutines.suspendCancellableCoroutine\n"
            "import kotlin.coroutines.resume\n"
            "import kotlin.coroutines.resumeWithException\n"
            "import java.util.concurrent.atomic.AtomicBoolean\n"
            "import java.util.concurrent.atomic.AtomicLong\n\n"
            "internal fun interface CrossaNativeCallback {\n"
            "    fun onComplete(state: Int, valueHandle: Long, errorHandle: Long)\n"
            "}\n\n"
            "internal object CrossaNativeBridge {\n"
            "    init { System.loadLibrary(\"crossa_runtime\") }\n"
            "    internal fun configure(overrides: CrossaConfigurationOverrides): Long = nativeConfigure(overrides.toNativeJson())\n"
            "    internal fun invokeAsync(runtime: Long, operationId: Long, arguments: Array<CrossaArgument>): Long = nativeInvokeAsync(runtime, operationId, arguments)\n"
            "    internal fun <T> invokeAsyncAfter(runtime: Long, operationId: Long, arguments: Array<CrossaArgument>, mapper: (CrossaNativeResult) -> T, onState: (CrossaState<T>) -> Unit): Long {\n"
            "        return nativeInvokeAsyncAfter(runtime, operationId, arguments, CrossaNativeCallback { state, valueHandle, errorHandle ->\n"
            "            when (state) {\n"
            "                0 -> {\n"
            "                    val result = CrossaNativeResult(runtime, valueHandle)\n"
            "                    try {\n"
            "                        onState(CrossaState.Success(mapper(result)))\n"
            "                    } catch (error: Throwable) {\n"
            "                        result.close()\n"
            "                        throw error\n"
            "                    }\n"
            "                }\n"
            "                1 -> {\n"
            "                    try {\n"
            "                        val message = nativeErrorMessage(runtime, errorHandle)\n"
            "                        val metadata = nativeErrorMetadata(runtime, errorHandle)\n"
            "                        val domain = metadata.toInt()\n"
            "                        val code = ((metadata ushr 32) and 0x7fffffffL).toInt()\n"
            "                        val retryable = ((metadata ushr 62) and 1L) != 0L\n"
            "                        onState(CrossaState.Failed(CrossaError(domain, code, message, retryable)))\n"
            "                    } finally {\n"
            "                        nativeReleaseError(runtime, errorHandle)\n"
            "                    }\n"
            "                }\n"
            "                else -> onState(CrossaState.Cancelled)\n"
            "            }\n"
            "        })\n"
            "    }\n"
            "    internal suspend fun <T> invokeAsyncAfterAwait(runtime: Long, operationId: Long, arguments: Array<CrossaArgument>, mapper: (CrossaNativeResult) -> T): T = suspendCancellableCoroutine { continuation ->\n"
            "        val operation = AtomicLong(0L)\n"
            "        val cancelled = AtomicBoolean(false)\n"
            "        val invocation = nativeInvokeAsyncAfter(runtime, operationId, arguments, CrossaNativeCallback { state, valueHandle, errorHandle ->\n"
            "            when (state) {\n"
            "                0 -> {\n"
            "                    val result = CrossaNativeResult(runtime, valueHandle)\n"
            "                    try {\n"
            "                        if (continuation.isActive) continuation.resume(mapper(result)) else result.close()\n"
            "                    } catch (error: Throwable) {\n"
            "                        result.close()\n"
            "                        if (continuation.isActive) continuation.resumeWithException(error)\n"
            "                    }\n"
            "                }\n"
            "                1 -> {\n"
            "                    try {\n"
            "                        val message = nativeErrorMessage(runtime, errorHandle)\n"
            "                        val metadata = nativeErrorMetadata(runtime, errorHandle)\n"
            "                        val error = CrossaError(metadata.toInt(), ((metadata ushr 32) and 0x7fffffffL).toInt(), message, ((metadata ushr 62) and 1L) != 0L)\n"
            "                        if (continuation.isActive) continuation.resumeWithException(error)\n"
            "                    } finally { nativeReleaseError(runtime, errorHandle) }\n"
            "                }\n"
            "                else -> if (continuation.isActive) continuation.cancel()\n"
            "            }\n"
            "        })\n"
            "        operation.set(invocation)\n"
            "        if (cancelled.get()) { nativeCancel(runtime, invocation); nativeReleaseOperation(runtime, invocation) }\n"
            "        continuation.invokeOnCancellation { cancelled.set(true); operation.get().takeIf { it != 0L }?.let { nativeCancel(runtime, it); nativeReleaseOperation(runtime, it) } }\n"
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
            "    internal fun valueKind(result: CrossaNativeResult, path: CrossaNativePath): Int = nativeValueKind(result.runtimeHandle(), result.requireHandle(), path.toPackedArray())\n"
            "    internal fun valueInt(result: CrossaNativeResult, path: CrossaNativePath): Int = nativeValueInt(result.runtimeHandle(), result.requireHandle(), path.toPackedArray())\n"
            "    internal fun valueLong(result: CrossaNativeResult, path: CrossaNativePath): Long = nativeValueLong(result.runtimeHandle(), result.requireHandle(), path.toPackedArray())\n"
            "    internal fun valueDouble(result: CrossaNativeResult, path: CrossaNativePath): Double = nativeValueDouble(result.runtimeHandle(), result.requireHandle(), path.toPackedArray())\n"
            "    internal fun valueBoolean(result: CrossaNativeResult, path: CrossaNativePath): Boolean = nativeValueBool(result.runtimeHandle(), result.requireHandle(), path.toPackedArray())\n"
            "    internal fun valueString(result: CrossaNativeResult, path: CrossaNativePath): String = nativeValueString(result.runtimeHandle(), result.requireHandle(), path.toPackedArray())\n"
            "    internal fun valueListSize(result: CrossaNativeResult, path: CrossaNativePath): Int = nativeValueListSize(result.runtimeHandle(), result.requireHandle(), path.toPackedArray())\n"
            "    internal fun valueJsonKind(result: CrossaNativeResult, path: CrossaNativePath): Int = nativeValueJsonKind(result.runtimeHandle(), result.requireHandle(), path.toPackedArray())\n"
            "    internal fun valueJsonBoolean(result: CrossaNativeResult, path: CrossaNativePath): Boolean = nativeValueJsonBool(result.runtimeHandle(), result.requireHandle(), path.toPackedArray())\n"
            "    internal fun valueJsonNumber(result: CrossaNativeResult, path: CrossaNativePath): String = nativeValueJsonNumber(result.runtimeHandle(), result.requireHandle(), path.toPackedArray())\n"
            "    internal fun valueJsonString(result: CrossaNativeResult, path: CrossaNativePath): String = nativeValueJsonString(result.runtimeHandle(), result.requireHandle(), path.toPackedArray())\n"
            "    internal fun valueJsonSize(result: CrossaNativeResult, path: CrossaNativePath): Int = nativeValueJsonSize(result.runtimeHandle(), result.requireHandle(), path.toPackedArray())\n"
            "    internal fun valueJsonKey(result: CrossaNativeResult, path: CrossaNativePath, field: Int): String = nativeValueJsonKey(result.runtimeHandle(), result.requireHandle(), path.toPackedArray(), field)\n"
            "    internal fun cancel(runtime: Long, operation: Long): Boolean = nativeCancel(runtime, operation)\n"
            "    internal fun releaseOperation(runtime: Long, operation: Long) { nativeReleaseOperation(runtime, operation) }\n"
            "    internal fun shutdown(runtime: Long) { nativeShutdown(runtime) }\n"
            "    internal fun releaseRuntime(runtime: Long) { nativeReleaseRuntime(runtime) }\n"
            "    private external fun nativeConfigure(overrides: String): Long\n"
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
            "    private external fun nativeErrorMetadata(runtime: Long, error: Long): Long\n"
            "    private external fun nativeReleaseError(runtime: Long, error: Long)\n"
            "    private external fun nativeResultBoolean(runtime: Long, result: Long): Boolean\n"
            "    private external fun nativeModelInt(runtime: Long, result: Long, model: Long, field: Int): Int\n"
            "    private external fun nativeModelLong(runtime: Long, result: Long, model: Long, field: Int): Long\n"
            "    private external fun nativeModelDouble(runtime: Long, result: Long, model: Long, field: Int): Double\n"
            "    private external fun nativeModelString(runtime: Long, result: Long, model: Long, field: Int): String\n"
            "    private external fun nativeModelBoolean(runtime: Long, result: Long, model: Long, field: Int): Boolean\n"
            "    private external fun nativeValueKind(runtime: Long, result: Long, path: IntArray): Int\n"
            "    private external fun nativeValueInt(runtime: Long, result: Long, path: IntArray): Int\n"
            "    private external fun nativeValueLong(runtime: Long, result: Long, path: IntArray): Long\n"
            "    private external fun nativeValueDouble(runtime: Long, result: Long, path: IntArray): Double\n"
            "    private external fun nativeValueBool(runtime: Long, result: Long, path: IntArray): Boolean\n"
            "    private external fun nativeValueString(runtime: Long, result: Long, path: IntArray): String\n"
            "    private external fun nativeValueListSize(runtime: Long, result: Long, path: IntArray): Int\n"
            "    private external fun nativeValueJsonKind(runtime: Long, result: Long, path: IntArray): Int\n"
            "    private external fun nativeValueJsonBool(runtime: Long, result: Long, path: IntArray): Boolean\n"
            "    private external fun nativeValueJsonNumber(runtime: Long, result: Long, path: IntArray): String\n"
            "    private external fun nativeValueJsonString(runtime: Long, result: Long, path: IntArray): String\n"
            "    private external fun nativeValueJsonSize(runtime: Long, result: Long, path: IntArray): Int\n"
            "    private external fun nativeValueJsonKey(runtime: Long, result: Long, path: IntArray, field: Int): String\n"
            "}\n"
        );
        writeFile(
            sourceDirectory / "runtime" / "CrossaRuntime.kt",
            "package " + packageName + ".runtime\n\n"
            "import " + packageName + ".internal.CrossaNativeBridge\n\n"
            "public object CrossaRuntime : AutoCloseable {\n"
            "    init { System.loadLibrary(\"crossa_runtime\") }\n\n"
            "    private var handle: Long = 0L\n\n"
            "    @Synchronized public fun configure(overrides: CrossaConfigurationOverrides = CrossaConfigurationOverrides()) {\n"
            "        require(handle == 0L) { \"Crossa runtime is already configured.\" }\n"
            "        handle = CrossaNativeBridge.configure(overrides)\n"
            "        check(handle != 0L) { \"Crossa native runtime creation failed.\" }\n"
            "    }\n\n"
            "    @Synchronized internal fun requireHandle(): Long = check(handle != 0L) { \"Crossa runtime is not configured.\" }.let { handle }\n\n"
            "    @Synchronized public fun shutdown() { if (handle != 0L) CrossaNativeBridge.shutdown(handle) }\n"
            "    @Synchronized override fun close() { if (handle != 0L) { val current = handle; handle = 0L; CrossaNativeBridge.releaseRuntime(current) } }\n"
            "}\n"
        );
        writeFile(
            sourceDirectory / "runtime" / "CrossaConfigurationOverrides.kt",
            "package " + packageName + ".runtime\n\n"
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
            "    )\n\n"
            "    internal fun toNativeJson(): String = buildString {\n"
            "        append('{')\n"
            "        var first = true\n"
            "        fun field(name: String, value: String) { if (!first) append(','); first = false; append(quote(name)); append(':'); append(value) }\n"
            "        baseUrl?.let { field(\"baseUrl\", quote(it)) }\n"
            "        timeoutRequest?.let { field(\"timeoutRequest\", it.toString()) }\n"
            "        commonHeaders?.let { headers -> field(\"commonHeaders\", headers.joinToString(prefix = \"[\", postfix = \"]\") { \"{\" + quote(\"name\") + \":\" + quote(it.name) + \",\" + quote(\"value\") + \":\" + quote(it.value) + \"}\" }) }\n"
            "        interceptor?.let { value -> field(\"interceptor\", buildString { append('{'); var nestedFirst = true; fun nested(name: String, nestedValue: String) { if (!nestedFirst) append(','); nestedFirst = false; append(quote(name)); append(':'); append(nestedValue) }; value.enabled?.let { nested(\"enabled\", it.toString()) }; value.logRequests?.let { nested(\"logRequests\", it.toString()) }; value.logResponses?.let { nested(\"logResponses\", it.toString()) }; value.logHeaders?.let { nested(\"logHeaders\", it.toString()) }; value.logBody?.let { nested(\"logBody\", it.toString()) }; value.excludedLogHeaders?.let { nested(\"excludedLogHeaders\", it.joinToString(prefix = \"[\", postfix = \"]\") { header -> quote(header) }) }; append('}') }) }\n"
            "        workerThreads?.let { field(\"workerThreads\", it.toString()) }\n"
            "        maxQueuedTasks?.let { field(\"maxQueuedTasks\", it.toString()) }\n"
            "        maxResponseBytes?.let { field(\"maxResponseBytes\", it.toString()) }\n"
            "        maxJsonDepth?.let { field(\"maxJsonDepth\", it.toString()) }\n"
            "        followRedirects?.let { field(\"followRedirects\", it.toString()) }\n"
            "        uploadProgress?.let { field(\"uploadProgress\", it.toString()) }\n"
            "        downloadStreaming?.let { field(\"downloadStreaming\", it.toString()) }\n"
            "        requestCoalescing?.let { field(\"requestCoalescing\", it.toString()) }\n"
            "        append('}')\n"
            "    }\n\n"
            "    private fun quote(value: String): String = buildString { append('\"'); value.forEach { character -> when (character) { '\\\\' -> append(\"\\\\\\\\\"); '\"' -> append(\"\\\\\\\"\"); '\\b' -> append(\"\\\\b\"); '\\u000C' -> append(\"\\\\f\"); '\\n' -> append(\"\\\\n\"); '\\r' -> append(\"\\\\r\"); '\\t' -> append(\"\\\\t\"); else -> if (character.code < 0x20) append(\"\\\\u%04x\".format(character.code)) else append(character) } }; append('\"') }\n"
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
        writeFile(
            outputDirectory / "library" / "src" / "main" / "cpp" /
                "CrossaGeneratedOperations.h",
            nativeGenerator.generateOperationHeader(programs)
        );
        writeFile(
            outputDirectory / "library" / "src" / "main" / "cpp" /
                "CrossaGeneratedProgram.h",
            nativeGenerator.generateProgramHeader()
        );
        writeFile(
            outputDirectory / "library" / "src" / "main" / "cpp" /
                "CrossaGeneratedProgram.cpp",
            nativeGenerator.generateProgramSource(programs)
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
            "target_compile_options(crossa_runtime PRIVATE -fvisibility=hidden -fvisibility-inlines-hidden -ffile-prefix-map=${CMAKE_CURRENT_LIST_DIR}=/crossa-source)\n"
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
            "#include <cstddef>\n"
            "#include <optional>\n"
            "#include <string_view>\n"
            "#include <utility>\n\n"
            "#include \"CrossaGeneratedProgram.h\"\n"
            "#include <crossa/bindings/android/AndroidJniBridge.h>\n"
            "#include <crossa/bindings/shared-abi/CrossaAbiRuntimeFactory.h>\n"
            "#include <crossa/network/json/JsonParser.h>\n"
            "#include <crossa/utils/Log.h>\n\n"
            "namespace {\n\n"
            "CrossaRuntimeHandle createGeneratedRuntime(const char* overrides, size_t overrideSize) noexcept {\n"
            "    try {\n"
            "        static const crossa::utils::Log log(crossa::utils::Log::Level::Debug);\n"
            "        CrossaRuntimeHandle runtime = 0;\n"
            "        crossa::compiler::ir::Program program = crossa::generated::CrossaGeneratedProgram::create();\n"
            "        std::optional<crossa::network::json::JsonValue> parsedOverrides;\n"
            "        if (overrides != nullptr && overrideSize != 0) {\n"
            "            parsedOverrides = crossa::network::json::JsonParser::parse(\n"
            "                std::string_view(overrides, overrideSize),\n"
            "                8U * 1024U * 1024U,\n"
            "                128\n"
            "            );\n"
            "        }\n"
            "        const CrossaStatus status = crossa::bindings::sharedabi::CrossaAbiRuntimeFactory::create(\n"
            "            std::move(program),\n"
            "            nullptr,\n"
            "            parsedOverrides == std::nullopt ? nullptr : &*parsedOverrides,\n"
            "            log,\n"
            "            &runtime\n"
            "        );\n"
            "        return status == CrossaStatusOk ? runtime : 0;\n"
            "    } catch (...) {\n"
            "        return 0;\n"
            "    }\n"
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
            "        \"" + packagePathValue + "/internal/CrossaNativeBridge\",\n"
            "        createGeneratedRuntime\n"
            "    ) ? JNI_VERSION_1_6 : JNI_ERR;\n"
            "}\n"
        );
    }

}
