#!/usr/bin/env bash

set -euo pipefail

projectRoot="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$projectRoot"

if command -v cmake >/dev/null 2>&1; then
    cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
    cmake --build build
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
else
    printf '%s\n' 'Build failed: cmake or c++ is required.' >&2
    exit 1
fi

./build/crossa test.cra --debug
#./build/crossa request.cra --debug
