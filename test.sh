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
    done < <(find src -type f -name '*.cpp' ! -path 'src/bindings/android/*' -print | sort)

    c++ \
        -std=c++20 \
        -Wall \
        -Wextra \
        -Wpedantic \
        -pthread \
        -Iinclude \
        -DCROSSA_SOURCE_DIRECTORY="\"$projectRoot\"" \
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

    languageTestSources=(
        tests/language-tests.cpp
        src/compiler/source/SourceFile.cpp
        src/compiler/source/SourceLocation.cpp
        src/compiler/source/SourceLoader.cpp
        src/compiler/lexer/TokenType.cpp
        src/compiler/lexer/Token.cpp
        src/compiler/lexer/Lexer.cpp
        src/compiler/ast/TypeReference.cpp
        src/compiler/ast/Expression.cpp
        src/compiler/ast/Statement.cpp
        src/compiler/ast/Declaration.cpp
        src/compiler/ast/SourceUnit.cpp
        src/compiler/ast/AstPrinter.cpp
        src/compiler/ast/CrossaRequestExpression.cpp
        src/compiler/ast/JsonExpression.cpp
        src/compiler/parser/Parser.cpp
        src/compiler/project/ProjectLinker.cpp
        src/compiler/generators/kotlin/KotlinGeneratedSource.cpp
        src/compiler/generators/kotlin/KotlinGenerator.cpp
        src/compiler/generators/kotlin/KotlinIdentifierEscaper.cpp
        src/compiler/generators/kotlin/KotlinProjectGenerationContext.cpp
        src/compiler/generators/kotlin/KotlinSourcePlanner.cpp
        src/compiler/generators/kotlin/KotlinExpressionEmitter.cpp
        src/compiler/generators/kotlin/KotlinSourceWriter.cpp
        src/compiler/generators/kotlin/KotlinStatementEmitter.cpp
        src/compiler/generators/kotlin/KotlinTypeMapper.cpp
        src/compiler/types/SemanticType.cpp
        src/compiler/semantic/SemanticScope.cpp
        src/compiler/semantic/TypedExpression.cpp
        src/compiler/semantic/TypedStatement.cpp
        src/compiler/semantic/TypedDeclaration.cpp
        src/compiler/semantic/TypedSourceUnit.cpp
        src/compiler/semantic/SemanticAnalyzer.cpp
        src/compiler/semantic/SemanticModelPrinter.cpp
        src/compiler/semantic/TypedCrossaRequestExpression.cpp
        src/compiler/semantic/TypedJsonExpression.cpp
        src/compiler/ir/IrExpression.cpp
        src/compiler/ir/IrStatement.cpp
        src/compiler/ir/IrDeclaration.cpp
        src/compiler/ir/IrLowerer.cpp
        src/compiler/ir/IrPrinter.cpp
        src/compiler/ir/IrCrossaRequestExpression.cpp
        src/compiler/ir/IrJsonExpression.cpp
        src/compiler/ir/Program.cpp
        src/utils/Log.cpp
        src/utils/PrintUtils.cpp
    )

    c++ \
        -std=c++20 \
        -Wall \
        -Wextra \
        -Wpedantic \
        -pthread \
        -Iinclude \
        "${languageTestSources[@]}" \
        -o build/crossa-language-tests

    ./build/crossa-language-tests
else
    printf '%s\n' 'Build failed: cmake or c++ is required.' >&2
    exit 1
fi

./build/crossa test.cra --debug
./build/crossa check examples/imports/runPosts.cra --debug
./build/crossa run tests/import-project/entry/runImports.cra
./build/crossa run tests/import-project/entry/runImports.cra
./build/crossa test tests/test-runner/pass.cra
./build/crossa test tests/conditionals.cra
./build/crossa tests/import-project/entry/runImports.cra --debug
./build/crossa tests/import-project/entry/runDiamondImports.cra --debug
./build/crossa examples/imports/repositories/postsRepository.cra --debug
./build/crossa tests/all-http-methods.cra
./build/crossa tests/config.cra
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
assertExecutionFails \
    "Assertion failed: intentional assertion failure" \
    ./build/crossa test tests/test-runner/failure.cra

if ! command -v socat >/dev/null 2>&1 && ! command -v ruby >/dev/null 2>&1; then
    printf '%s\n' 'Test failed: socat or ruby is required for local network integration.' >&2
    exit 1
fi
bash scripts/run-local-network-tests.sh ./build/crossa

if [[ "${CROSSA_RUN_NETWORK_INTEGRATION:-0}" == "1" ]]; then
    ./build/crossa run tests/network-jsonplaceholder.cra
    ./build/crossa run examples/imports/runPosts.cra
fi
