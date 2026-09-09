# Crossa Testing

Crossa uses two test layers:

- `crossa_language_tests` is a dependency-free C++ unit-test executable covering lexer tokens, parser/AST, semantic types and diagnostics, IR lowering, and recursive project linking.
- `crossa_runtime_tests` covers native result state, typed response decoding, structured errors, scheduler cancellation, and shutdown behavior.
- `crossa_kotlin_generator_test_suite` invokes `generate kotlin` against golden
  fixtures for scalar types, operators, control flow, `@Sync`, `Unit`,
  `print`, identifier/string escaping, parameter wrapping, and configured
  packages. It also verifies clear rejection of invalid packages,
  `CrossaRequest`, `@Async`, `@AsyncAfter`, models, and `List<T>`.
- CTest runs the real `crossa` executable against `test.cra`, imported scripts,
  nested project fixtures, explicit `check`, `run`, and `test` commands, and
  expected CLI failures.

Use `check` for a side-effect-free validation pass. It loads the complete import
graph and stops after typed IR lowering, so it does not load `config.cra` and
does not execute top-level functions or requests. `run` continues through
native execution. `test` continues through native execution, requires at least
one `assert`, and returns a non-zero status for failed assertions or runtime
errors.

Run the complete local suite from the repository root:

```bash
./test.sh
```

When CMake is installed, `test.sh` configures `build/`, builds all targets, and runs CTest. When CMake is unavailable, it uses the native C++ compiler and still runs both unit-test executables plus the CLI integration fixtures. `scripts/run-kotlin-generator-tests.sh` covers Kotlin golden-file generation, and `scripts/run-android-project-generator-tests.sh` validates the generated Android project and embedded dependency policy.

Host sanitizer builds are opt-in CMake configurations:

```bash
cmake -S . -B build/asan -DCROSSA_ENABLE_ASAN=ON
cmake --build build/asan --parallel
ctest --test-dir build/asan --output-on-failure
cmake -S . -B build/ubsan -DCROSSA_ENABLE_UBSAN=ON
cmake --build build/ubsan --parallel
ctest --test-dir build/ubsan --output-on-failure
cmake -S . -B build/tsan -DCROSSA_ENABLE_TSAN=ON
```

Address, undefined behavior, and thread sanitizers are mutually exclusive. The `asan` and `ubsan` configurations are independent so each gate reports its own failures.

GitHub Actions runs the native suite, Kotlin generator validation, and an
AddressSanitizer and UndefinedBehaviorSanitizer builds on every push and pull request through:

```text
.github/workflows/ci.yml
```

The default test fixtures do not depend on a live network service. The runnable
JSONPlaceholder example remains available at `examples/imports/runPosts.cra`,
but it is intentionally not part of the deterministic CI suite.

CrossaRequest integration tests use JSONPlaceholder and are opt-in because they
depend on external network availability. Enable them with:

```bash
CROSSA_RUN_NETWORK_INTEGRATION=1 ./test.sh
```

The integration suite covers a direct typed `Post` response with headers and
query parameters, plus the imported `List<Post>` request chain in
`examples/imports/runPosts.cra`.
