#!/usr/bin/env bash

set -euo pipefail

projectRoot="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$projectRoot"

# Verifies that one CLI execution fails with the expected diagnostic text.
assertExecutionFails() {
    local expectedMessage="$1"
    shift
    local executionOutput

    if executionOutput="$("$@" 2>&1)"; then
        printf '%s\n' "Expected command to fail: $*" >&2
        exit 1
    fi
    if [[ "$executionOutput" != *"$expectedMessage"* ]]; then
        printf '%s\n' "Expected diagnostic containing: $expectedMessage" >&2
        printf '%s\n' "$executionOutput" >&2
        exit 1
    fi
}

if command -v cmake >/dev/null 2>&1; then
    cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
    cmake --build build
    ctest --test-dir build --output-on-failure
elif command -v c++ >/dev/null 2>&1; then
    mkdir -p build
    sourceFiles=()
    curlFlags=(-lcurl)

    if command -v curl-config >/dev/null 2>&1; then
        read -r -a curlFlags <<< "$(curl-config --libs)"
    fi

    while IFS= read -r sourceFile; do
        sourceFiles+=("$sourceFile")
    done < <(find src -type f -name '*.cpp' -print | sort)

    c++ \
        -std=c++20 \
        -Wall \
        -Wextra \
        -Wpedantic \
        -pthread \
        -Iinclude \
        "${sourceFiles[@]}" \
        "${curlFlags[@]}" \
        -o build/crossa

    c++ \
        -std=c++20 \
        -Wall \
        -Wextra \
        -Wpedantic \
        -pthread \
        -Iinclude \
        tests/native-runtime-tests.cpp \
        src/compiler/ir/IrDeclaration.cpp \
        src/compiler/ir/Program.cpp \
        src/compiler/source/SourceLocation.cpp \
        src/compiler/types/SemanticType.cpp \
        src/network/json/JsonParser.cpp \
        src/network/json/JsonSerializer.cpp \
        src/network/json/JsonValue.cpp \
        src/network/response/ResponseDecoder.cpp \
        src/runtime/RequestHandle.cpp \
        src/runtime/RuntimeValue.cpp \
        src/runtime/errors/CrossaError.cpp \
        src/runtime/errors/CrossaException.cpp \
        src/runtime/objects/NativeList.cpp \
        src/runtime/objects/NativeModel.cpp \
        src/runtime/scheduler/ScheduledTask.cpp \
        src/runtime/scheduler/SchedulerOptions.cpp \
        src/runtime/scheduler/TaskScheduler.cpp \
        src/utils/Log.cpp \
        src/utils/PrintUtils.cpp \
        -o build/crossa-runtime-tests

    ./build/crossa-runtime-tests
else
    printf '%s\n' 'Build failed: cmake or c++ is required.' >&2
    exit 1
fi

./build/crossa test.cra --debug
./build/crossa tests/import-project/entry/runImports.cra --debug
./build/crossa tests/import-project/entry/runDiamondImports.cra --debug
./build/crossa examples/imports/repositories/postsRepository.cra --debug
assertExecutionFails \
    "was not found under project root" \
    ./build/crossa tests/import-errors/missing/missingImportEntry.cra
assertExecutionFails \
    "is ambiguous" \
    ./build/crossa tests/import-errors/ambiguous/entry/ambiguousImportEntry.cra
assertExecutionFails \
    "Circular import detected" \
    ./build/crossa tests/import-errors/cycle/cycleOne.cra
assertExecutionFails \
    "top-level execution belongs to the entry file" \
    ./build/crossa tests/import-errors/execution/importedExecutionEntry.cra
assertExecutionFails \
    "Imports must appear before all declarations" \
    ./build/crossa tests/import-errors/order/importAfterDeclaration.cra
#./build/crossa request.cra --debug
