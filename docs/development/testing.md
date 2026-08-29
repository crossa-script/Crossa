# Crossa Testing

Crossa uses two test layers:

- `crossa_language_tests` is a dependency-free C++ unit-test executable covering lexer tokens, parser/AST, semantic types and diagnostics, IR lowering, and recursive project linking.
- `crossa_runtime_tests` covers native result state, typed response decoding, structured errors, scheduler cancellation, and shutdown behavior.
- CTest runs the real `crossa` executable against `test.cra`, imported scripts, nested project fixtures, and expected CLI failures.

Run the complete local suite from the repository root:

```bash
./test.sh
```

When CMake is installed, `test.sh` configures `build/`, builds all targets, and runs CTest. When CMake is unavailable, it uses the native C++ compiler and still runs both unit-test executables plus the CLI integration fixtures.

GitHub Actions runs the same script on every push and pull request through:

```text
.github/workflows/ci.yml
```

The test fixtures do not depend on a live network service. The runnable JSONPlaceholder example remains available at `examples/imports/runPosts.cra`, but it is intentionally not part of the deterministic CI suite.
