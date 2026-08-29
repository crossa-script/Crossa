#!/usr/bin/env bash

set -euo pipefail

if [[ "$#" -ne 1 ]]; then
    printf '%s\n' 'Usage: run-kotlin-generator-tests.sh <crossa>' >&2
    exit 1
fi

crossaBinary="$1"
scriptDirectory="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
projectRoot="$(cd "$scriptDirectory/.." && pwd)"

fail() {
    printf '%s\n' "$1" >&2
    exit 1
}

runGeneration() {
    "$crossaBinary" generate kotlin "$1" --output "$2"
}

verifySuccessCase() {
    local name="$1"
    local sourcePath="$2"
    local expectedPath="$3"
    local outputDirectory
    outputDirectory="$(mktemp -d)"

    if ! runGeneration "$sourcePath" "$outputDirectory"; then
        rm -rf "$outputDirectory"
        fail "Kotlin $name generation failed."
    fi

    local generatedPath="$outputDirectory/$(basename "${sourcePath%.cra}").kt"
    if [[ ! -f "$generatedPath" ]]; then
        rm -rf "$outputDirectory"
        fail "Kotlin $name generation did not create $(basename "$generatedPath")."
    fi
    if ! cmp -s "$generatedPath" "$expectedPath"; then
        rm -rf "$outputDirectory"
        fail "Kotlin $name generation did not match the golden output."
    fi
    if [[ "$(find "$outputDirectory" -mindepth 1 -maxdepth 1 -type f | wc -l | tr -d ' ')" != "1" ]]; then
        rm -rf "$outputDirectory"
        fail "Kotlin $name generation created unexpected output files."
    fi
    rm -rf "$outputDirectory"
}

verifyFailureCase() {
    local name="$1"
    local sourcePath="$2"
    local expectedDiagnostic="$3"
    local outputDirectory
    outputDirectory="$(mktemp -d)"
    local output

    if output="$(runGeneration "$sourcePath" "$outputDirectory" 2>&1)"; then
        rm -rf "$outputDirectory"
        fail "Kotlin generation accepted $name."
    fi
    if [[ "$output" != *"$expectedDiagnostic"* ]]; then
        rm -rf "$outputDirectory"
        fail "Kotlin $name did not produce the expected diagnostic."
    fi
    rm -rf "$outputDirectory"
}

verifySuccessCase \
    'package' \
    "$projectRoot/tests/kotlin-generator/Math.cra" \
    "$projectRoot/tests/kotlin-generator/Math.kt"
verifySuccessCase \
    'scalars' \
    "$projectRoot/tests/kotlin-generator-scalars/Scalars.cra" \
    "$projectRoot/tests/kotlin-generator-scalars/Scalars.kt"
verifySuccessCase \
    'flow' \
    "$projectRoot/tests/kotlin-generator-flow/Flow.cra" \
    "$projectRoot/tests/kotlin-generator-flow/Flow.kt"
verifySuccessCase \
    'escaping' \
    "$projectRoot/tests/kotlin-generator-escaping/when.cra" \
    "$projectRoot/tests/kotlin-generator-escaping/when.kt"
verifyFailureCase \
    'an invalid package' \
    "$projectRoot/tests/kotlin-generator-invalid-package/Invalid.cra" \
    'config packageName'
verifyFailureCase \
    'a native request' \
    "$projectRoot/tests/kotlin-generator-invalid-request/Request.cra" \
    "runtime-backed expression 'CrossaRequest'"
verifyFailureCase \
    'an asynchronous function' \
    "$projectRoot/tests/kotlin-generator-invalid-async/Async.cra" \
    'execution policies other than Sync'
verifyFailureCase \
    'an asynchronous completion function' \
    "$projectRoot/tests/kotlin-generator-invalid-async-after/AsyncAfter.cra" \
    'execution policies other than Sync'
verifyFailureCase \
    'a model' \
    "$projectRoot/tests/kotlin-generator-invalid-model/User.cra" \
    'top-level declaration'
verifyFailureCase \
    'a list' \
    "$projectRoot/tests/kotlin-generator-invalid-list/Lists.cra" \
    "does not support type 'List<Int>'"

printf '%s\n' 'Crossa Kotlin generator CLI tests passed'
