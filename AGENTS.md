# Repository Guidelines

## Project Structure

Crossa is a C++20 compiler and native runtime for `.cra` sources. Production code is under `src/`, public headers under `include/crossa/`, and the executable entry point is `src/main.cpp`. Compiler stages live in `src/compiler/`; runtime, networking, CLI, packaging, and platform bindings are separated into their respective directories. Unit tests are in `tests/*.cpp`, language and CLI fixtures are in `tests/` and `examples/`, and Android/iOS examples are in `android-example/` and `ios-example/`. Architecture and workflow documentation belongs under `docs/`; read `ARCHITECTURE.md` before changing compiler, IR, runtime, ABI, ownership, concurrency, or platform behavior.

## Build and Development Commands

Requirements are CMake, Ninja, a C++20 compiler, and libcurl.

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build
./build/crossa run test.cra
./test.sh
```

`test.sh` builds the project, runs CTest, executes native unit tests, and validates CLI fixtures. Use `CROSSA_RUN_NETWORK_INTEGRATION=1 ./test.sh` only when external JSONPlaceholder access is available. Kotlin and Android generator checks are available through `scripts/run-kotlin-generator-tests.sh` and `scripts/run-android-project-generator-tests.sh`.

## Coding Style and Naming

Use four spaces, never tabs. Follow existing C++ naming and namespace conventions: PascalCase types, lowerCamelCase variables and functions, and matching paths under `src/` and `include/crossa/`. Keep workflows in focused functions, preserve deterministic generated output, and avoid new dependencies or architectural patterns without justification. Do not add comments to code unless the task specifically requires them.

## Testing Guidelines

Add or update focused C++ tests for compiler and runtime changes, and add `.cra` fixtures for language or CLI behavior. Name fixtures by behavior, such as `conditionals.cra` or `import-errors/`. Run `./test.sh` before submitting; use ASan or TSan CMake builds for memory or concurrency changes.

## Commits and Pull Requests

Recent commits use short imperative summaries with a category prefix, for example `Fix: ...`, `Update ...`, or `Optimize ...`. Keep commits focused. Pull requests should explain behavior and scope, link the relevant issue or ADR when applicable, list validation commands, and include generated-output or platform screenshots when UI or artifact changes need visual review.

## Documentation and Configuration

Update the nearest document under `docs/` when behavior or architecture changes. Put architectural decisions in `docs/decisions/`. Keep credentials, local SDK paths, and generated build directories out of commits; use existing configuration examples and environment variables for machine-specific settings.
