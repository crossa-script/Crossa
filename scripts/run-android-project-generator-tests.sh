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

generatedProject="$outputDirectory/project"
generatedSources="$generatedProject/library/src/main/kotlin/com/example/crossa"

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
requireFile "$generatedProject/library/build.gradle.kts"
requireFile "$generatedProject/library/src/main/AndroidManifest.xml"
requireFile "$generatedProject/library/src/main/cpp/CMakeLists.txt"
requireFile "$generatedProject/library/src/main/cpp/crossa_runtime.cpp"
requireFile "$generatedSources/Math.kt"
requireFile "$generatedSources/CrossaRuntime.kt"
requireFile "$generatedSources/CrossaConfigurationOverrides.kt"

cmp "$generatedSources/Math.kt" "$projectRoot/tests/kotlin-generator/Math.kt"
requireText "$generatedProject/library/build.gradle.kts" 'namespace = "com.example.crossa"'
requireText "$generatedProject/library/src/main/cpp/CMakeLists.txt" '-Wl,-z,max-page-size=16384'
requireText "$generatedProject/library/src/main/cpp/CMakeLists.txt" '-Wl,-z,common-page-size=16384'
requireText "$generatedSources/CrossaRuntime.kt" 'fun configure(overrides: CrossaConfigurationOverrides)'
requireText "$generatedSources/CrossaConfigurationOverrides.kt" 'data class CrossaConfigurationOverrides('
requireText "$generatedSources/CrossaConfigurationOverrides.kt" 'data class Interceptor('

printf '%s\n' 'Crossa Android project generator tests passed'
