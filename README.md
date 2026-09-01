# Crossa

Crossa is a compiler-powered native runtime and scripting/code-generation platform. It transforms backend contracts and `.cra` source files into high-performance native SDKs and platform APIs for Android and iOS.

Crossa is not a general-purpose programming language or another platform networking client. Its intentionally small language describes typed behavior, while the C++ compiler and runtime own performance-critical execution.

## Project Status

> **Foundation phase:** Crossa currently contains its architecture and language specifications, the canonical C++ frontend, typed IR, native IR execution, a bounded shared scheduler, native HTTP transport, JSON request/response support, schema-aware response validation, Android Gradle project generation, and standalone CLI release packaging. XCFramework output, complete generated bindings, cancellation, and streaming remain in progress.

The frontend validates variables, models, config blocks, functions, execution policies, top-level calls, returns, arithmetic, lexical scopes, `List<T>`, `Json`, interpolated strings, and `CrossaRequest`, then lowers them to platform-neutral IR. The native interpreter executes only reachable calls; declarations without calls are compiled but remain inert.

The first production runtime module will be Networking. Future modules may include WebSockets, raw and binary sockets, Database, Streaming, Cache, Compression, Cryptography, File Transport, and Telemetry.

## How Crossa Fits Together

```text
Backend Contracts                 .cra Sources
       |                               |
       v                               v
Backend Adapters              C++ Language Frontend
       |                    Lexer / Parser / Semantics
       +---------------+---------------+
                       |
                       v
                  Shared Crossa IR
                       |
                       v
              Optimization / Linking
                       |
            +----------+----------+
            |                     |
            v                     v
      Native C++ Runtime    Kotlin / Swift APIs
            |
            v
       Stable Native ABI
            |
       Android / iOS
```

Different inputs share one semantic, IR, runtime, and generator architecture. Kotlin and Swift remain thin integration surfaces for runtime-backed behavior.

## The `.cra` Language

Crossa source files use the `.cra` extension. The initial language includes typed variables, functions, models, `List<T>`, string interpolation, execution policies, and native request expressions.

```cra
model User(
    id: Int,
    name: String
)

@AsyncAfter
fun getUsers(id: Int): List<User> {
    re CrossaRequest {
        url: "/v1/users/#id",
        method: GET,
        queryParams: {
            include: "profile"
        }
    }
}
```

Here, `List<User>` is the logical success type. `CrossaRequest` lowers into a native request plan, executes through the shared C++ scheduler and Networking runtime, and produces `Success(data)` or `Failed(error)` semantics for generated platform APIs.

## Native-First Architecture

For runtime-backed features, C++ owns:

- Request planning, encoding, transport, cancellation, and timeouts.
- Response buffering, schema-aware parsing, and native model storage.
- Memory ownership, scheduling, errors, and async state.
- The canonical `.cra` frontend and language-to-IR lowering.

Crossa does not generate separate Retrofit, Ktor, OkHttp, or URLSession implementations for `CrossaRequest`. Large results should remain native-backed where practical to reduce allocations, copies, JNI/Swift crossings, and managed object duplication.

## Documentation

- [Technical Architecture](ARCHITECTURE.md) — system boundaries and engineering rules.
- [Agent Instructions](AGENTS.md) — repository workflow and coding-agent requirements.
- [Language Foundation](docs/language/language-foundation.md) — current `.cra` syntax and semantics.
- [Language Roadmap](docs/language/language-roadmap.md) — implementation order and milestones.
- [Project Blueprint](CROSSA_PROJECT_BLUEPRINT.md) — broader product direction; current architecture and language documents take precedence where the blueprint is outdated.

## Installation

Crossa installs from compiled GitHub Release artifacts. The installers do not
clone this repository, build Crossa locally, require `sudo`, or modify shell
profile files.

### macOS ARM64

Install the latest stable release:

```sh
curl -fsSL https://raw.githubusercontent.com/crossa-script/Crossa/main/scripts/install/install.sh | bash
```

Install a specific version:

```sh
curl -fsSL https://raw.githubusercontent.com/crossa-script/Crossa/main/scripts/install/install.sh | bash -s -- 0.1.0
```

The macOS installer downloads:

```text
crossa-vX.Y.Z-macos-arm64.tar.gz
```

### Linux x86_64

Install the latest stable release:

```sh
curl -fsSL https://raw.githubusercontent.com/crossa-script/Crossa/main/scripts/install/install.sh | bash
```

Install a specific version:

```sh
curl -fsSL https://raw.githubusercontent.com/crossa-script/Crossa/main/scripts/install/install.sh | bash -s -- 0.1.0
```

The Linux installer downloads:

```text
crossa-vX.Y.Z-linux-x86_64.tar.gz
```

### Windows x86_64

Install the latest stable release:

```powershell
irm https://raw.githubusercontent.com/crossa-script/Crossa/main/scripts/install/install.ps1 | iex
```

Install a specific version:

```powershell
Invoke-WebRequest https://raw.githubusercontent.com/crossa-script/Crossa/main/scripts/install/install.ps1 -OutFile install.ps1
.\install.ps1 -Version 0.1.0
```

The Windows installer downloads:

```text
crossa-vX.Y.Z-windows-x86_64.zip
```

macOS and Linux install to:

```text
~/.crossa/bin/crossa
```

Windows installs to:

```text
%USERPROFILE%\.crossa\bin\crossa.exe
```

Each installer downloads `SHA256SUMS` from the same GitHub Release and verifies
the archive before extracting it. If the Crossa bin directory is not on `PATH`,
the installer prints the exact command to add it.

Crossa installation flow:

```text
GitHub Release
      |
      v
install.sh
install.ps1
      |
      v
Detect OS + CPU
      |
      v
Resolve release version
      |
      v
Download matching binary archive
      |
      v
Verify SHA-256
      |
      v
~/.crossa/bin/crossa
      |
      v
crossa doctor
```

## Supported Platforms

| Platform | Architecture | Release Asset | Installer |
|----------|--------------|---------------|-----------|
| macOS | ARM64 / Apple Silicon | `crossa-vX.Y.Z-macos-arm64.tar.gz` | `install.sh` |
| Linux | x86_64 | `crossa-vX.Y.Z-linux-x86_64.tar.gz` | `install.sh` |
| Windows | x86_64 | `crossa-vX.Y.Z-windows-x86_64.zip` | `install.ps1` |

## Verify Installation

```sh
crossa --version
crossa doctor
```

`crossa doctor` checks the Crossa installation and the Android development
toolchain required for generated Android AAR builds.

## CLI Commands

| Command | What it does | Example |
|---------|--------------|---------|
| `crossa <file.cra>` | Runs a source file; equivalent to `crossa run`. | `crossa examples/imports/runPosts.cra` |
| `crossa run <file.cra>` | Executes reachable top-level calls. | `crossa run file.cra` |
| `crossa check <file.cra>` | Validates source, imports, semantics, and IR without execution. | `crossa check examples/imports/runPosts.cra` |
| `crossa test <file.cra>` | Runs a source with assertion semantics. | `crossa test tests/test-runner/pass.cra` |
| `crossa generate kotlin <file.cra> --output <directory>` | Generates deterministic Kotlin for pure translated IR. | `crossa generate kotlin tests/kotlin-generator/Math.cra --output generated/` |
| `crossa generate-build android <project-directory> --output <directory> [--ndk-version <version>] [--gradle-version <version>] [--kotlin-version <version>]` | Generates an Android Gradle library project for AAR assembly, with optional per-project tool versions. | `crossa generate-build android ./crossa-project --output ./build/crossa-aar --ndk-version 28.1.13356709 --gradle-version 8.11.1 --kotlin-version 2.0.21` |
| `crossa doctor` | Checks the default Android build toolchain on the current machine. | `crossa doctor` |
| `crossa --version` | Prints the installed Crossa version. | `crossa --version` |

### CLI Examples

Run a `.cra` file with the default `run` command:

```sh
crossa examples/imports/runPosts.cra
```

Run, validate, or test a source explicitly:

```sh
crossa run test.cra
crossa check examples/imports/runPosts.cra
crossa test tests/test-runner/pass.cra
```

Show compiler pipeline details while executing a source:

```sh
crossa run --debug test.cra
```

Generate pure Kotlin output:

```sh
crossa generate kotlin tests/kotlin-generator/Math.cra --output ./generated
```

Generate an Android library with Crossa's default tool versions:

```sh
crossa generate-build android ./crossa-project --output ./build/crossa-aar
```

Generate an Android library with project-specific NDK, Gradle, and Kotlin versions:

```sh
crossa generate-build android ./crossa-project --output ./build/crossa-aar \
  --ndk-version 28.1.13356709 \
  --gradle-version 8.11.1 \
  --kotlin-version 2.1.10
```

Inspect the installation, print the version, or view CLI usage:

```sh
crossa doctor
crossa --version
crossa --help
```

The Android version flags are used only by `generate-build android`: NDK is
written to `library/build.gradle.kts`, Gradle selects the wrapper distribution,
and Kotlin selects the Kotlin Android plugin. When omitted, Crossa uses its
current defaults. Choose compatible Gradle, Kotlin, Android Gradle Plugin, and
NDK versions for your project.

## Release Artifacts

Example release assets for `Crossa v0.1.0`:

```text
crossa-v0.1.0-macos-arm64.tar.gz
crossa-v0.1.0-linux-x86_64.tar.gz
crossa-v0.1.0-windows-x86_64.zip
SHA256SUMS
```

## Development

Configure, build, and run the current executable with CMake and Ninja:

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build
./build/crossa test.cra
```

The executable accepts an optional command and one `.cra` source file. `run`
validates, lowers, and executes top-level calls in source order through the
shared scheduler and native network runtime. `test` uses the same native runtime
with test-only `assert` calls and exits non-zero on an assertion or runtime
failure. `check` loads imports and runs the lexer, parser, semantic analysis,
and typed IR lowering, then stops without loading configuration or executing
requests and functions. Normal execution displays program output and errors,
without compiler lifecycle logs:

```sh
./build/crossa test.cra
```

Use explicit commands when the intent should be clear:

```sh
./build/crossa check examples/imports/runPosts.cra
./build/crossa run file.cra
./build/crossa test file.cra
./build/crossa generate kotlin tests/kotlin-generator/Math.cra --output generated/
./build/crossa doctor
```

`generate kotlin` stops after typed IR lowering and writes one deterministic
`<SourceIdentity>.kt` file to the requested output directory. When the source
directory has a `config.cra` with `packageName: "com.example.app"`, the
generated file begins with that Kotlin package directive. It supports only the
pure Kotlin IR backend; runtime-backed operations such as `CrossaRequest` fail
explicitly rather than generating platform networking code.

The command is optional for backward compatibility, so `crossa file.cra` is
equivalent to `crossa run file.cra`.

Use `crossa doctor` to inspect whether the current machine has the Crossa
installation, Android SDK, NDK, CMake, Ninja, Java, cache, and temporary
directory setup required for generated Android AAR builds. See
[Crossa CLI](docs/development/cli.md) for the doctor status meanings.

Test sources use assertions:

```cra
fun returnsTrue(): Bool {
    re true
}

assert(returnsTrue(), "returnsTrue should pass")
```

Use `--debug` to display every current Crossa execution step, emitted token, parsed AST declaration, typed semantic declaration, and lowered IR instruction:

```sh
./build/crossa --debug test.cra
```

Put a call in the `.cra` file to execute it through the native IR interpreter:

```cra
fun add(a: Int, b: Int): Int {
    re a + b
}

print(add(1, 2))
```

```sh
./build/crossa test.cra
```

Use `config.cra` beside the executing source for shared networking and runtime
settings. Absolute HTTP/HTTPS request URLs bypass `baseUrl`; relative URLs use
it. See [Native Networking](docs/features/networking.md) for request fields,
header precedence, response decoding, limits, and current exclusions.

To configure, build, and run the debug test in one command:

```sh
./test.sh
```

The script builds and runs the complete compiler, linker, runtime, local mock
network, and `.cra` scripting suite. It uses CMake when available and falls back
to the installed C++ compiler. Native networking requires libcurl.

Optional JSONPlaceholder integration tests can be enabled when network access is
available:

```sh
CROSSA_RUN_NETWORK_INTEGRATION=1 ./test.sh
```

With CMake, the same tests are enabled with
`-DCROSSA_ENABLE_NETWORK_INTEGRATION=ON`.

See [Crossa Testing](docs/development/testing.md) for the test layers and fixtures.

Compiler implementation follows the documented vertical slices: source loading, diagnostics, lexer, parser and AST, semantic analysis, typed IR, native execution, native Networking, then deterministic platform generators.

Architecture, ABI, IR, ownership, scheduler, transport, parser, memory-layout, module-boundary, and major dependency changes require an ADR under `docs/decisions/`.

## License

Crossa is available under the [Apache License 2.0](LICENSE).
