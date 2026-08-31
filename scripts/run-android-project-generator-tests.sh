#!/usr/bin/env bash

set -euo pipefail

if [[ "$#" -ne 1 ]]; then
    printf '%s\n' 'Usage: run-android-project-generator-tests.sh <crossa>' >&2
    exit 1
fi

crossaBinary="$1"
scriptDirectory="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
projectRoot="$(cd "$scriptDirectory/.." && pwd)"
outputDirectory="$(mktemp -d)"
trap 'rm -rf "$outputDirectory"' EXIT

"$crossaBinary" generate-build android \
    "$projectRoot/tests/kotlin-generator" \
    --output "$outputDirectory/project"
"$crossaBinary" generate-build android \
    "$projectRoot/tests/android-network-generator" \
    --output "$outputDirectory/network-project"

generatedProject="$outputDirectory/project"
generatedSources="$generatedProject/library/src/main/kotlin/com/example/crossa"
generatedNetworkProgram="$outputDirectory/network-project/library/src/main/cpp/CrossaGeneratedProgram.cpp"

requireFile() {
    if [[ ! -f "$1" ]]; then
        printf '%s\n' "Missing generated Android file: $1" >&2
        exit 1
    fi
}

requireText() {
    if ! rg -F --quiet -- "$2" "$1"; then
        printf '%s\n' "Generated Android file does not contain expected text: $2" >&2
        exit 1
    fi
}

requireFile "$generatedProject/settings.gradle.kts"
requireFile "$generatedProject/build.gradle.kts"
requireFile "$generatedProject/gradlew"
requireFile "$generatedProject/gradlew.bat"
requireFile "$generatedProject/gradle/wrapper/gradle-wrapper.jar"
requireFile "$generatedProject/gradle/wrapper/gradle-wrapper.properties"
requireFile "$generatedProject/library/build.gradle.kts"
requireFile "$generatedProject/library/consumer-rules.pro"
requireFile "$generatedProject/library/src/main/AndroidManifest.xml"
requireFile "$generatedProject/library/src/main/cpp/CMakeLists.txt"
requireFile "$generatedProject/library/src/main/cpp/CrossaAndroidDependencies.cmake"
requireFile "$generatedProject/library/src/main/cpp/CrossaOpenSslInstall.cmake"
requireFile "$generatedProject/library/src/main/cpp/crossa_runtime.cpp"
requireFile "$generatedSources/Math.kt"
requireFile "$generatedSources/CrossaRuntime.kt"
requireFile "$generatedSources/CrossaConfigurationOverrides.kt"

cmp "$generatedSources/Math.kt" "$projectRoot/tests/kotlin-generator/Math.kt"
requireText "$generatedProject/library/build.gradle.kts" 'namespace = "com.example.crossa"'
requireText "$generatedProject/library/src/main/cpp/CMakeLists.txt" '-Wl,-z,max-page-size=16384'
requireText "$generatedProject/library/src/main/cpp/CMakeLists.txt" '-Wl,-z,common-page-size=16384'
requireText "$generatedProject/library/src/main/cpp/CMakeLists.txt" 'CROSSA_ANDROID_EMBEDDED_CA_BUNDLE=1'
requireText "$generatedProject/library/src/main/cpp/CrossaAndroidDependencies.cmake" '-ffile-prefix-map='
requireText "$generatedProject/library/src/main/cpp/CrossaAndroidDependencies.cmake" 'openssl-3.0.15.tar.gz'
requireText "$generatedProject/library/src/main/cpp/CrossaAndroidDependencies.cmake" 'curl-8.12.1.tar.xz'
requireText "$generatedProject/library/src/main/cpp/CrossaAndroidDependencies.cmake" 'cacert-2025-02-25.pem'
requireText "$generatedProject/library/src/main/cpp/CrossaAndroidDependencies.cmake" 'SHA256=23c666d0edf20f14249b3d8f0368acaee9ab585b09e1de82107c66e1f3ec9533'
requireText "$generatedProject/library/src/main/cpp/CrossaAndroidDependencies.cmake" 'SHA256=0341f1ed97a26c811abaebd37d62b833956792b7607ea3f15d001613c76de202'
requireText "$generatedProject/library/src/main/cpp/CrossaAndroidDependencies.cmake" 'SHA256=50a6277ec69113f00c5fd45f09e8b97a4b3e32daa35d3a95ab30137a55386cef'
requireText "$generatedSources/CrossaRuntime.kt" 'fun configure(overrides: CrossaConfigurationOverrides)'
requireText "$generatedSources/CrossaConfigurationOverrides.kt" 'data class CrossaConfigurationOverrides('
requireText "$generatedSources/CrossaConfigurationOverrides.kt" 'data class Interceptor('
requireText "$generatedNetworkProgram" 'make_unique<IrCrossaRequestExpression>'
requireText "$generatedNetworkProgram" 'IrHttpMethod::Get'

if [[ -e "$generatedProject/library/src/main/cpp/crossa/src/bindings/android/AndroidUnavailableCurlTransport.cpp" ]]; then
    printf '%s\n' 'Generated Android project still contains the unavailable curl fallback.' >&2
    exit 1
fi

printf '%s\n' 'Crossa Android project generator tests passed'
