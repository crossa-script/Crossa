# Crossa

Crossa is a compiler-powered native runtime and scripting/code-generation platform. It transforms backend contracts and `.cra` source files into high-performance native SDKs and platform APIs for Android and iOS.

Crossa is not a general-purpose programming language or another platform networking client. Its intentionally small language describes typed behavior, while the C++ compiler and runtime own performance-critical execution.

## Project Status

> **Foundation phase:** Crossa currently contains its architecture and language specifications, the canonical C++ frontend, typed IR, native IR execution, a bounded shared scheduler, native HTTP transport, JSON request/response support, and schema-aware response validation. The AAR, XCFramework, generated bindings, cancellation, streaming, and release pipelines are not yet implemented.

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
- [Language Foundation](language-foundation.md) — current `.cra` syntax and semantics.
- [Language Roadmap](language-roadmap.md) — implementation order and milestones.
- [Project Blueprint](CROSSA_PROJECT_BLUEPRINT.md) — broader product direction; current architecture and language documents take precedence where the blueprint is outdated.

## Development

Configure, build, and run the current executable with CMake and Ninja:

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build
./build/crossa test.cra
```

The executable accepts one `.cra` source file and automatically compiles a sibling `config.cra` when present. It validates, lowers, and executes top-level calls in source order through the shared scheduler and native network runtime. Normal execution displays program output and errors, without compiler lifecycle logs:

```sh
./build/crossa test.cra
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

The script builds and runs the complete compiler, linker, runtime, and `.cra` scripting suite. It uses CMake when available and falls back to the installed C++ compiler. Native networking requires libcurl. GitHub Actions repeats this workflow on every push and pull request.

See [Crossa Testing](docs/development/testing.md) for the test layers and fixtures.

Compiler implementation follows the documented vertical slices: source loading, diagnostics, lexer, parser and AST, semantic analysis, typed IR, native execution, native Networking, then deterministic platform generators.

Architecture, ABI, IR, ownership, scheduler, transport, parser, memory-layout, module-boundary, and major dependency changes require an ADR under `docs/decisions/`.

## License

Crossa is available under the [Apache License 2.0](LICENSE).
