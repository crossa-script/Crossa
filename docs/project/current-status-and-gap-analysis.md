# Crossa Current Status and Gap Analysis

**Audit date:** 2026-09-06  
**Auditor role:** Principal compiler / runtime / mobile SDK / release / repository audit  
**Nature of this document:** evidence-based status of the codebase as it exists. This is not a roadmap and does not modify architecture.  
**Code freeze during audit:** no production code, tests, CI, APIs, ABI, IR, or packaging pipelines were changed. Temporary artifact inspection used `/tmp/crossa-audit-*` only.

---

## 1. Executive Summary

Crossa is a C++20 compiler and native runtime that turns `.cra` sources into a linked typed IR, executes that IR on a bounded native scheduler (including libcurl HTTP), and generates thin Android (AAR) and iOS (XCFramework) APIs over a small C ABI.

The **compiler/language V0 vertical slice is real and mostly closed**. Host execution of multi-file projects, `@AsyncAfter` native requests, JSON-to-`NativeModel`/`NativeList` decoding, and CLI `check`/`run`/`test`/`generate`/`generate-build`/`doctor` all exist in production code and were exercised in this audit.

The **Android and iOS consumer examples are real external repositories**. They consume packaged artifacts (local debug AAR / local XCFramework), not Crossa C++ sources. Both contain competitor comparison UIs (Retrofit+OkHttp+Ktor on Android, Alamofire on iOS). Artifact inspection confirms native libraries, generated APIs, and the expected ABI slices.

What is **not** closed:

- **Distribution is not closed.** There are no Git tags and GitHub Releases return 404. `install.sh` / `install.ps1` cannot obtain a binary without cloning and building.
- **The loopback local-network harness was removed.** `scripts/run-local-network-tests.sh` (and its mock server helpers and `tests/local-network/` fixtures) is no longer part of `./test.sh` or CTest. Opt-in JSONPlaceholder tests remain.
- **Android native-backed models cannot represent nested `Model` or `List` fields.** The Kotlin generator throws `native-backed Android model field type`. iOS already supports nested fields via ABI value paths. This is the largest Android/iOS architecture-parity gap.
- **Several authoritative documents are stale** relative to code and later ADRs. Roadmap Phase 7/10/12/18 banners are the worst offenders. They must not be used as implementation truth.

**Verdicts**

| Question | Verdict |
|---|---|
| Is the compiler/language V0 milestone closed? | **READY WITH NON-BLOCKING GAPS** |
| Is Android closed end-to-end? | **READY WITH NON-BLOCKING GAPS** |
| Is iOS closed end-to-end? | **READY WITH NON-BLOCKING GAPS** |
| Is the compiler/runtime pipeline closed end-to-end on host? | **READY WITH NON-BLOCKING GAPS** |
| Is packaging/distribution closed? | **NOT READY** |

**Overall current compiler/mobile milestone readiness: 81%.**  
**Distribution readiness: 32%.**

The correct immediate next phase is **not** a new language feature, adapter, benchmark program, or WebSocket/database module. It is to put the Kotlin/Android generator scripts on the default `test.sh`/CI path that documentation already claims, then close Android nested native views so the ABI path already used by iOS is actually reachable from JNI.

---

## 2. Audit Scope

### In scope

- All Git repositories reachable from `/Users/yazantarifi/Crossa` and reasonable parent directories.
- Authority documents listed in §4.
- C++ compiler frontend, project linker, semantic analysis, IR, native generation, runtime, scheduler, networking, ABI.
- Kotlin generator, Android JNI, AAR packaging, `android-example`.
- Swift generator, iOS bridge, XCFramework / SwiftPM, `ios-example`.
- CLI, doctor, installers, CI, release workflows, existing tests, existing artifacts.

### Out of scope (not counted as current defects)

Database, WebSocket, raw/binary sockets, cache, crypto, backend adapters, loops, switch, nullable syntax, enums, user-defined generics, inheritance, reflection, HTTP/3, curl-multi event loop, generated direct schema decoders as a *production optimization* (they are explicitly a later stage).

### Method

Documentation was read first and treated as intent. Implementation status was assigned only from source, build wiring, packaging wiring, existing artifacts, and commands actually executed. `Unverified` is never promoted to `Implemented`.

---

## 3. Repository Inventory

The workspace `/Users/yazantarifi/Crossa` is **not itself a Git repository**. It is a directory of three sibling Git repositories. No additional Crossa Git repositories were found under `/Users/yazantarifi` (depth 3) or `/Users/yazantarifi/Code` (depth 4).

| Repository | Path | Responsibility | Branch | HEAD | Remote | Dirty? | Build system | CI | Status |
|---|---|---|---|---|---|---|---|---|---|
| **Crossa** | `/Users/yazantarifi/Crossa/Crossa` | Compiler, runtime, CLI, generators, Android/iOS packaging | `main` | `1d499bf0cdfe5564c521f3511e2ae4b186a358fb` | `git@github.com:crossa-script/Crossa.git` | Clean | CMake 3.22 + Ninja (host); generated Gradle/CMake (Android); generated Xcode (iOS) | `.github/workflows/ci.yml`, `release.yml` | Active core |
| **android-example** | `/Users/yazantarifi/Crossa/android-example` | External Android consumer + Retrofit/Ktor comparison | `main` | `13fa938acec1fc501040b71c0c4a867651e57af4` | `git@github.com:crossa-script/android-example.git` | Clean | Gradle (AGP) | None | Active example |
| **ios-example** | `/Users/yazantarifi/Crossa/ios-example` | External SwiftUI consumer + Alamofire comparison | `main` | `33b18be47a552594087f4d69c9143e5c4f599eaf` | `git@github.com:crossa-script/ios-example.git` | Clean | Xcode + local SwiftPM binaryTarget | None | Active example |

Latest commits (all 2026-09-06, author Yazan Tarifi): Crossa `Update Readme file`; android-example `New Build`; ios-example `New Update`.

**No Git tags exist in any repository.**

### Crossa (core)

- **Languages:** C++20 (102 `.cpp`, 104 public headers), `.cra` fixtures (46), Kotlin/Swift/CMake/Gradle/Xcode *generated* by the CLI, Bash/PowerShell installers.
- **Dependency system:** `find_package(CURL REQUIRED)`, `Threads`; Android generated projects pin OpenSSL 3.0.15 + curl 8.12.1 + Mozilla CA; iOS generated projects pin static libcurl with Apple Security TLS.
- **CLI version:** `0.1.0` hardcoded in `CrossaVersion::current()`.
- **Tests:** `tests/language-tests.cpp`, `tests/native-runtime-tests.cpp`, CLI fixtures, `scripts/run-*.sh`.
- **Examples:** `examples/imports/` (model → request → repository → entry).
- **Installers:** `scripts/install/install.sh`, `scripts/install/install.ps1`.
- **CI:** host C++ build/test on `ubuntu-latest`; release workflow for macOS ARM64 / Linux x86_64 / Windows x86_64 CLI archives.
- **Generated/binary artifacts present locally (untracked build trees, not source of truth):** `build-host/crossa`, many `build/android-final-v*` AARs, `build/crossa-ios/`, vendored `build/android-sdk/` (NDK 28.1.13356709, CMake 3.22.1).
- **Missing subsystem docs that AGENTS.md still routes to:** `docs/project/product.md`, `docs/engineering/engineering-principles.md`, `docs/architecture/compiler.md`, `docs/compiler/ir.md`, `docs/architecture/runtime.md`, `docs/architecture/abi.md`. `docs/architecture/` does not exist.

### android-example

- Kotlin/Compose app `com.crossa.androiddemo`, minSdk 23, compileSdk 35.
- Consumes `app/libs/crossa-generated-debug.aar` via `implementation(files(...))`.
- Generation: `scripts/generate-crossa-aar.sh` → `crossa generate-build android ./crossa` then Gradle `assembleDebug`.
- `.cra` graph: `crossa/{config.cra,Post.cra,postRequests.cra,postsRepository.cra}`.
- Comparison: Retrofit 2.11 + OkHttp, Ktor 2.3.12 OkHttp engine, Crossa `@AsyncAfter`.
- No CI. LICENSE + README. Screenshot of an emulator run dated 2026-09-06.
- Built APK present: `app/build/outputs/apk/debug/app-debug.apk` (25 MB, 2026-09-06).

### ios-example

- SwiftUI app, iOS 14 deployment (framework claims iOS 13).
- Consumes `CrossaBinary/Crossa.xcframework` through local `CrossaPackage/Package.swift` `binaryTarget(path:)`.
- Install: `scripts/install-crossa-framework.sh`.
- Same `.cra` graph as Android.
- Comparison: Alamofire 5.12.0 via SPM.
- No CI. LICENSE + README + screenshot.

---

## 4. Source-of-Truth and Documentation Authority

Authority order used (as required):

1. Nearest `AGENTS.md` for agent/repository process.
2. `ARCHITECTURE.md` for technical architecture.
3. Subsystem docs and ADRs `docs/decisions/0001`–`0009`.
4. `docs/language/language-foundation.md` for `.cra` V0.
5. `docs/language/language-roadmap.md` for progression claims.
6. `CROSSA_PROJECT_BLUEPRINT.md` for historical product direction.
7. README / example READMEs.

**Conflicts are reported in §32. They are documentation drift unless code independently confirms a defect.** After this audit, the stale “not done” status banners in `docs/language/language-roadmap.md`, `docs/features/networking.md`, `AGENTS.md`, `ARCHITECTURE.md` iOS ABI wording, `README.md` generated-API row, `docs/platform/android.md`, and `language-foundation.md` §71 were updated to match implementation. Remaining planned items (direct schema decoders, platform streaming ABI, adapters, optimizer) were left planned.

Practical constitution for implementers:

- Process: `AGENTS.md` — but its language freeze-list and “no committed build configuration” sentences are stale.
- Architecture: `ARCHITECTURE.md` + ADRs 0001–0009 + `docs/platform/android.md` + `docs/platform/ios.md` + `docs/runtime/*` + `docs/features/networking.md`.
- Language: `language-foundation.md`.
- Treat the blueprint as historical. Treat several roadmap “Implementation status” banners as stale ledgers.

---

## 5. Current End-to-End Architecture

Reconstructed from **code**, not diagrams.

```text
.cra file(s)
    SourceLoader::load
    Lexer::tokenize
    Parser::parse                          → per-file AST
    ProjectLinker::link                    → linked project AST
    SemanticAnalyzer::analyze              → TypedSourceUnit
    IrLowerer                              → compiler::ir::Program (per identity)
    ┌──────────────────────────────────────┴──────────────────────────────────────┐
    │ Native path                                                                  │
    │   NativeProgramGenerator  → reconstructed IR in generated C++                │
    │   NativeRuntime           → TaskScheduler + IrInterpreter + NetworkEngine    │
    │   CurlTransport           → pooled libcurl easy handles                      │
    │   ResponseDecoder         → JsonParser DOM → NativeModel / NativeList        │
    │   CrossaAbi               → opaque handles, no exceptions across boundary    │
    └─────────────────────────────────────────────────────────────────────────────┘
    Generators (consume linked IR, do not parse .cra)
        KotlinGenerator::generate          → pure Kotlin (CLI `generate kotlin`)
        KotlinGenerator::generateProject   → Android native-binding Kotlin
        AndroidProjectGenerator            → Gradle library + JNI + CMake + deps
        SwiftGenerator::generateProject    → Swift models + CrossaFunctions
        IosProjectGenerator                → Xcode + archive script + SPM templates
```

There is **no backend-adapter frontend** in this workspace. Architecture still draws “Backend Contracts → Adapters → Contract”; that path is planned, not implemented.

There is **no IR optimizer** directory or pass. Architecture names “Optimization and Linking”; linking of `.cra` imports is real; optimization is not.

Host CLI and generated mobile artifacts share the same frontend, IR, runtime, and ABI. Kotlin/Swift do not implement HTTP.

---

## 6. End-to-End Pipeline Status

| Stage | Status | Evidence |
|---|---|---|
| `.cra` files | Implemented | 46 fixtures; example graphs in all three repos |
| Source loading | Implemented | `SourceLoader`; size limits in `CompilerResourceLimits`; `.cra` extension enforced |
| UTF-8 validation | Partially Implemented | Files loaded as bytes; no well-formed UTF-8 checker found |
| Import / project discovery | Implemented | `ProjectLinker`; CLI `ProjectSourceDiscovery`; tests for missing/ambiguous/cycle/order/execution |
| Lexer | Implemented | `Lexer`; language tests `verifyLexerTokens`; tokens are ranges, not copied lexemes |
| Parser / per-file AST | Implemented | `Parser`; AST nodes for imports, functions, models, config, if, JSON, CrossaRequest |
| Project linker | Implemented | Recursive index, cycles, ambiguity, diamond; `check examples/imports/runPosts.cra --debug` linked 4 modules / 5 declarations |
| Semantic analysis | Implemented | `SemanticAnalyzer`; language tests plus live `check` |
| Type system | Implemented | `SemanticType` includes Unit, Int, Long, Double, String, Bool, Json, Model, List |
| Typed representation | Implemented | `TypedSourceUnit` / typed expressions/statements |
| Crossa IR | Implemented | `IrLowerer`, `Program`; request plans, interpolation builds, execution policies |
| IR linking / canonicalization | Partially Implemented | Generators index multiple `Program*` and de-dupe models; no separate optimizer; native generator emits one reconstructed program |
| Native generation | Implemented | `NativeProgramGenerator::generateProgramSource(vector<const Program*>)` |
| Native runtime | Implemented | `NativeRuntime`, `ExecutionEngine`, `IrInterpreter` |
| Scheduler | Implemented | `TaskScheduler` bounded workers/queue; runtime tests for queued/executing/shutdown cancellation |
| CrossaRequest | Implemented | AST → semantic → IR → interpreter `evaluateRequest` |
| Native networking | Implemented | `NetworkEngine` + `CurlTransport`; live `crossa test tests/network-jsonplaceholder.cra` passed |
| Response buffering | Implemented | Bounded `HttpResponse`; config `maxResponseBytes` |
| Response decoding | Partially Implemented | Functional schema-aware decoder over a temporary JSON DOM; not a generated direct decoder |
| Native model/list ownership | Implemented | `NativeModel`, `NativeList`, `RuntimeValue` via `shared_ptr<const ...>` |
| Stable ABI | Implemented | `CrossaAbi.h` version 1; opaque handles; `catch (...)` at boundary |
| Android JNI | Implemented | `AndroidJniBridge.cpp` `JNI_OnLoad` + `RegisterNatives`; 24/24 match |
| iOS bridge | Implemented | `crossaIosCreateGeneratedRuntime`; Swift wrappers in `IosProjectGenerator` |
| Generated Kotlin API | Implemented | Project-aware `api/` + `model/` + runtime; pure CLI generator also exists |
| Generated Swift API | Implemented | `CrossaFunctions` extensions + native-backed `Post` / `CrossaList` in the installed XCFramework |
| AAR | Implemented (debug verified; release exists in older build trees) | Example debug AAR 2.6 MB, `jni/arm64-v8a/libcrossa_runtime.so`, 16 KB ELF alignment |
| XCFramework | Implemented | `ios-arm64` + `ios-arm64-simulator`, dSYMs, Swift module |
| Example applications | Partially Implemented / Unverified in this session | Artifacts and source wired; Android APK exists; this audit did not relaunch emulator/simulator UI |
| Backend adapters | Planned | No adapter sources |
| IR optimizer | Planned | No optimizer sources |

---

## 7. Language Frontend Status

### Source system — Implemented

| Concern | Finding |
|---|---|
| Ownership | `SourceFile` owns path + content string |
| Locations | `SourceLocation` stores shared path pointer + line/column |
| Ranges | `Token` is offset/length into the source buffer; `getLexeme` returns `string_view` |
| Size limits | `MaximumSourceBytes = 16 MiB` |
| Diagnostics | Source-aware failures via exceptions internally, mapped to CLI errors; codes such as `CRA1001` on size |
| Malformed input | Unterminated strings fail in the lexer; missing files fail in the loader |

UTF-8 is the documented encoding. The loader does not reject invalid UTF-8 sequences. Treat as a hardening gap, not a missing language feature.

### Lexer — Implemented

`TokenType` covers V0: identifiers, integer/decimal literals, strings, booleans, `import` paths, keywords (`fun`, `re`, `if`, `else`, `var`, `model`, `config`, `print`, `assert`, `Int`/`Long`/`Double`/`String`/`Bool`/`List`/`Json`/`null`/`CrossaRequest`), annotations `@Sync`/`@Async`/`@AsyncAfter`, all nine HTTP method literals, operators including `== != <= >= && || ! + - * /`, brackets, EOF.

Strings are one `StringLiteral` token. `#identifier` interpolation is **not** a dedicated lexer token family; it is parsed from the string later. Roadmap Phase 2 explicitly allows this representation.

`#filename.cra#` import paths are a dedicated `ImportPath` token (`scanImportPath`).

Token ownership does not copy source text.

### Parser — Implemented for V0

Parsed constructs observed in `Parser.h` / language tests / fixtures:

imports, functions, parameters, return types, variables, models, model fields, config, `if` / `else if` / `else`, comparisons, boolean operators, arithmetic, calls, `re`, `print`, `assert`, JSON objects/arrays, `CrossaRequest` entries, annotations, `List<T>`.

**Documented but absent (correctly — not V0):** loops, switch, nullable syntax, enums, maps, packages, visibility, user generics.

**Implemented but weakly specified in foundation grammar:** `Long` and `Double` as first-class scalar types (architecture and `SemanticType` include them; foundation §8 type list and EBNF `scalar_type` omit them; foundation §14.2 mentions them in comparisons). This is a spec hole, not a missing parser.

### Frontend tests

`crossa_language_tests` passed on `build-host/crossa_language_tests`. Cases: lexer, parser AST, conditionals, import position, semantic model, numeric types, semantic failure, IR lowering, Kotlin generation, project linking, linking failures.

---

## 8. Project Linker / Multi-File Status

**Status: Implemented** (host-verified).

`ProjectLinker::link` indexes every regular `.cra` under the project root by **exact filename**, resolves `import #filename.cra#`, detects missing / ambiguous / circular imports, visits each module once, preserves source locations, and concatenates declarations in deterministic traversal order. Limits: 10 000 files, 256 MiB project bytes, 1024 imported modules, depth 256.

Verified in this audit:

```text
$ build-host/crossa check examples/imports/runPosts.cra --debug
Project linking completed: 4 source modules, 5 declarations
AST/Semantic/IR Model Post, Model PostAuthor
IR Function fetchPosts policy=AsyncAfter return=List<Post>
IR Function getPosts policy=AsyncAfter return=List<Post>  (Call fetchPosts)
IR Top-level Evaluate Call(print, Call(getPosts))
```

That is exactly:

```text
Model in file A (domain/models.cra)
  → used by request in file B (network/postRequests.cra)
    → used by repository in file C (repositories/postsRepository.cra)
      → called from entry file D (runPosts.cra)
```

as **one linked program**.

Additional host checks that passed:

- `crossa run tests/import-project/entry/runImports.cra`
- `crossa tests/import-project/entry/runDiamondImports.cra`
- expected failures: missing, ambiguous, circular, imported top-level execution, import-after-declaration

Imported files are declarations-only at the entry-execution boundary (imported top-level calls are rejected). `config.cra` is reserved and is not emitted as a Kotlin/Swift API class.

---

## 9. Semantic Analysis and Type System Status

**Status: Implemented** for V0.

`SemanticAnalyzer` registers top-level names, models, and function signatures first, then types bodies. It validates duplicate symbols, unknown symbols/types, parameter/argument counts and types, returns, `List<T>`, model fields, execution-policy combinations, config keys, request fields, and interpolation identifiers.

Built-in types in code: Unit, Int, Long, Double, String, Bool, Json, Model, List.

Interpolation is resolved against lexical/function/source scope at semantic time (`analyzeStringExpression`, `analyzeRequestUrl`). Runtime does not rescan `.cra` text.

### Duplicated semantics — mostly conformant

| Concern | Owner in code | Leakage? |
|---|---|---|
| Lexing/parsing | C++ only | None found in Kotlin/Swift |
| Type checking | `SemanticAnalyzer` | Generators map already-resolved `SemanticType`; they do not re-parse `.cra` |
| Interpolation | Semantic → IR `IrStringBuildExpression` | Runtime evaluates the plan |
| Request meaning | Semantic → IR `IrCrossaRequestExpression` | Network engine executes prepared specs |
| Kotlin/Swift API shape | Generators | Thin wrappers; Android JNI field accessors are mechanical |
| Response schema | Function return type in IR, consumed by `ResponseDecoder` | Platforms do not parse JSON |

Android example and iOS example **do** implement independent Retrofit/Ktor/Alamofire stacks for comparison. That is example-app code, not generated Crossa output.

---

## 10. Crossa IR Status

**Status: Implemented** and platform-neutral.

`compiler::ir::Program` is one source identity plus ordered `IrDeclaration`s (models, functions, config, top-level evaluates). Observed IR concepts: functions, parameters, locals, constants, return, evaluate, binary/unary, conditionals, calls, string-build interpolation plans, JSON, `CrossaRequest` (method, URL plan, headers, query, body, timeout, policies), execution policy, expected response type.

`check examples/imports/runPosts.cra --debug` printed:

```text
IR Return CrossaRequest(method=GET, url=StringBuild(Literal("https://jsonplaceholder.typicode.com/posts")), ...)
IR Function getPosts ... Return Call(fetchPosts)
IR Top-level Evaluate Call(print, Call(getPosts))
```

**Architectural leakage into core IR:** none found. IR headers/sources do not mention JNI, Kotlin, Swift, Gradle, or Xcode.

**Duplicate model/function behavior:** Swift and Kotlin project generators key models by name and functions by source location; conflicting duplicate symbols throw. Linker + semantic analysis are the first line of defense.

**IR linking:** generators and `NativeProgramGenerator` accept `vector<const Program*>` and emit one native reconstruction. There is no separate “IR optimizer” pass.

---

## 11. Native Generation / Execution Status

**Status: Implemented** (one coherent native program per project).

`NativeProgramGenerator` emits:

- stable FNV-1a operation IDs from source identity + function signature
- C++ that reconstructs the validated IR (`CrossaGeneratedProgram`)

Android and iOS packaging embed that reconstruction plus runtime sources. Host CLI does not need generated C++; it interprets the in-memory IR via `ExecutionEngine` → `IrInterpreter`.

`IrInterpreter` executes scalars, locals, calls, print, assert (test mode only), conditionals, JSON, and native requests. `@Async` / `@AsyncAfter` go through `TaskScheduler`.

Determinism: operation IDs are pure hashes; Kotlin/Swift emission sorts imports and uses maps/sets keyed by stable names. Language tests include Kotlin golden files.

Host proof:

- `crossa test.cra` printed `3` (pure `add`)
- `crossa test tests/conditionals.cra` passed
- `crossa test tests/network-jsonplaceholder.cra` passed (live HTTP + model decode + assert)

---

## 12. Runtime and Scheduler Status

**Status: Implemented** with bounded resources; a few architecture tensions noted below.

| Topic | Implementation |
|---|---|
| Lifecycle | `NativeRuntime` construct / `shutdown`; ABI `crossaRuntimeShutdown` + `crossaReleaseRuntime` |
| Ownership | Runtime owns configuration, `NetworkEngine`, `TaskScheduler`, interpreter, operation catalog, `CrossaRuntimeContext` |
| Scheduler | `TaskScheduler`: worker pool, `maximumQueuedTasks`, `deque` queue, shutdown/join, `isWorkerThread` |
| Cancellation | `RequestHandle`; tests for queued, executing, and shutdown cancellation |
| Exactly-once completion | `runTask` produces one `CrossaState`; ABI completion callback is one-shot from JNI/Swift |
| Errors | `CrossaError` / `CrossaException` internally; ABI integer status + error handles |
| Handle registry | `CrossaAbiRuntimeRegistry` with worker-originated reaper (ADR 0008) |
| Logging | `utils::Log`; network debug includes curl replay |
| Metrics | Request telemetry events (`telemetry event=completed ... downloadChunks=`) when enabled |

### Forbidden-pattern search

| Pattern | Result |
|---|---|
| Mutable global runtime application state | `curl_global_init` in `CurlTransport` is process-global (libcurl requirement). ABI registry is a process-local singleton of handles, not application config. Android `CrossaRuntime` Kotlin `object` is a process singleton. |
| One thread per operation | Not used; bounded pool |
| Unbounded queues | Queue bounded by `maxQueuedTasks` |
| Unbounded buffers | `maxResponseBytes` / header limits |
| Exceptions crossing ABI | `CrossaAbi.cpp` wraps in `catch (...)` and returns `CrossaStatus` |
| Raw owning pointers | Not the dominant pattern; `unique_ptr` for IR/AST, `shared_ptr` for models/lists/runtime handles |
| `shared_ptr` proliferation | Present where shared views are required (models, lists, ABI results, coalesced requests, `RequestHandle` state). This matches ADR 0002, not accidental utility sharing |
| Platform-specific runtime core | Networking/runtime C++ is shared; JNI and iOS files are bindings |

Lifetime: ABI string views are valid until the result/error handle is released. Kotlin `CrossaNativeResult.close()` and Swift owner deinit are the platform release paths.

---

## 13. CrossaRequest / Networking Status

**Status: Implemented** for the V0 request/config surface that the **code** actually runs. Roadmap Phase 18 “auth/multipart/retry still planned” is **false**.

### Configuration

`RuntimeConfiguration` / `NetworkConfiguration` consume `config.cra` keys including `packageName`, `baseUrl`, `timeoutRequest`, `commonHeaders`, `interceptor`, `workerThreads`, `maxQueuedTasks`, `maxResponseBytes`, `maxJsonDepth`, header limits, `followRedirects`, `retryPolicy`, `authProviders`, `uploadProgress`, `downloadStreaming`, `requestCoalescing`, `proxy`, `certificatePolicy`, `telemetry`.

Foundation §25 lists this full set. Roadmap Phase 13 lists a subset. **Code matches foundation + ADR 0004, not Phase 13.**

### Request lowering

Implemented in AST/IR/interpreter: `url`/`path`, interpolation plans, `pathVariables`, `queryParams`, `headers`, `customHeaders`, `body` (JSON), `timeout`, all nine HTTP methods, `retryPolicy`, `auth`, `multipart`, `proxy`, `certificatePolicy`, `telemetry`, `uploadProgress`, `downloadStreaming`, `coalesce`.

### Native transport

`CurlTransport`: libcurl **easy** handle **pool** (not curl-multi). Connection reuse via pooled handles. TLS: host uses system/libcurl defaults; Android generated builds pin OpenSSL 3.0.15 + `CURLOPT_CAINFO_BLOB`; iOS uses Apple Security TLS. Timeouts, redirects, cancellation via `RequestHandle`, bounded response buffer, mime multipart (`CURLOPT_MIMEPOST`).

ADR 0001 explicitly deferred curl-multi. Blueprint still talks as if multi/HTTP/2 is V1 transport — drift.

### Response decoding

`ResponseDecoder::decode` parses with Crossa-owned `JsonParser` (bounded bytes/depth) into a generic `JsonValue` DOM, then recursively constructs `RuntimeValue` / `NativeModel` / `NativeList` from the IR schema, then the DOM is not retained (except explicit `Json` results).

This is a **functional schema-aware decoder**, not the final generated direct decoder. README correctly labels generated direct decoders “in progress.”

Nested models: decoder walks IR field types recursively (native side). Platform exposure of nested fields is Android-limited (see §14).

### Validation

| Command | Result |
|---|---|
| `crossa test tests/network-jsonplaceholder.cra` | **Passed** (live GET Post, headers, query, assert) |
| `crossa test examples/imports/runPosts.cra` | Executed live `List<Post>` (100 posts printed) then failed because the example has **no `assert`** — expected `test` semantics, not a network failure |
| `scripts/run-local-network-tests.sh` | **Failed** after successful GET list / POST / auth / multipart / telemetry: missing substring `/posts/1?page=1` in curl logs |

Interpretation: native HTTP + JSON model/list decode **work**. The local harness’s curl-replay assertion for `getPost()` query URL is **Broken**. Query parameters themselves work on JSONPlaceholder (`queryParams: { integration: "true" }`). Do not conclude query encoding is unimplemented.

Auth refresh, retry (test expects two `GET /status/500` attempts), and multipart were exercised in the same local run (debug log shows auth attempt=2, multipart `-F` curl, retry fixture not reached because the script aborted earlier).

---

## 14. Native Model and List Status

**Status: Implemented natively; Android platform views are scalar-field only; iOS views support nested model/list.**

| Capability | Native C++ | Android Kotlin | iOS Swift |
|---|---|---|---|
| Native-backed model | `NativeModel` | `class Post(owner, handle)` | `struct Post { nativeValue }` |
| Native-backed `List<T>` | `NativeList` | `CrossaNativeList` for `List<Model>` results | `CrossaList<Element>` |
| Primitive field access | yes | JNI per field index | ABI path `child(field:)` |
| String field access | yes | JNI `nativeModelString` | `.string()` |
| Nested model field | stored in `RuntimeValue` | **Generator `failUnsupported`** | `Model(nativeValue: child)` |
| Nested list field | stored | **Generator `failUnsupported`** | `CrossaList(nativeValue: child, map:)` |
| Indexed collection | yes | `nativeListModelHandle` at **root list** | `element(index:)` path |
| Explicit release | ABI `crossaReleaseResult` | `CrossaNativeResult.close()` / `AutoCloseable` | owner deinit; scalars `take*()` then release |
| Materialization | not default in runtime | Example **eagerly copies** to domain `Post` | Example wraps `CrossaList<Post>` in presentation data; field access still native |

Android generator evidence (`KotlinGenerator::emitNativeModel`):

```text
SemanticTypeKind::Model / List → failUnsupported("native-backed Android model field type")
```

Android `emitNativeFunction` supports:

- `List<Model>` → `CrossaNativeList`
- root `Model`
- scalar Int/Long/Double/String/Bool
- **not** `List<Int>`, **not** `Json`, **not** nested fields

iOS `SwiftGenerator::modelFieldValue` emits nested model/list via `CrossaNativeValue` path segments that call ABI `crossaGetPath*`.

**JNI does not bind the ABI path accessors.** Nested Android support cannot be finished in Kotlin alone.

Current examples only use scalar `Post` fields and `List<Post>` as a function result, so the demo path works on both platforms.

---

## 15. Stable ABI Status

**Status: Implemented** (v1), small, C, versioned.

Public header: `include/crossa/bindings/shared-abi/CrossaAbi.h`

```text
#define CROSSA_ABI_VERSION 1U
```

Exports (C): runtime create (deprecated without generated program; factory used instead), release, shutdown; `crossaInvokeAsync` / `crossaInvokeAsyncAfter`; cancel/release operation; result/error release; scalar result getters; list size / list model / root model; per-field model scalars; **value-path** kind/size/scalar getters; error message/metadata.

Properties:

- Opaque `uint64_t` handles
- No STL in the ABI
- No C++ types in the public struct surface (`CrossaStringView` is `const char*` + `size_t`)
- Exceptions caught at the ABI (`catch (...)`)
- Explicit release operations
- Capability/version: compile-time `CROSSA_ABI_VERSION`; iOS artifact manifest records `runtimeAbiVersion: 1`

Android JNI is a concentrated wrapper over this ABI, not a second runtime. iOS Swift wrappers in `IosProjectGenerator` call the same C functions.

iOS installed binary: Swift public API only; C ABI symbols are not a user-facing Swift surface.

**Not yet a long-term compatibility machine:** no additive ABI test suite, no version negotiation at runtime beyond the macro, no published stability window.

---

## 16. Kotlin Generator Status

Two backends:

| Backend | CLI | Status |
|---|---|---|
| Pure translation | `crossa generate kotlin <file> --output <dir>` | Implemented for Sync scalar IR; **rejects** `CrossaRequest`, `@Async`, `@AsyncAfter`, models, `List<T>` |
| Android native binding | `crossa generate-build android` via `generateProject` | Implemented, project-aware |

Project-aware behavior (Android):

- Indexes complete linked IR
- Canonicalizes models once under `model/`
- Functions stay with source-unit under `api/`
- Shared `runtime/` and `internal/CrossaNativeBridge.kt`
- Deterministic import sorting
- `config.cra` is not a Kotlin class
- `packageName` from config

Validated: `scripts/run-kotlin-generator-tests.sh` **passed**. Invalid request generation exits 1 with `Kotlin generation does not support runtime-backed expression 'CrossaRequest'.`

There is **no** generated OkHttp/Retrofit/Ktor client.

---

## 17. Android JNI Status

**Count (generated `CrossaNativeBridge` vs `AndroidJniBridge.cpp` `JNINativeMethod[]`):**

| Metric | N |
|---|---|
| Kotlin `external` methods | **24** |
| JNI `RegisterNatives` entries | **24** |
| Matched | **24** |
| Missing | **0** |
| Extra JNI entries | **0** |

Methods: `nativeConfigure`, `nativeInvokeAsync`, `nativeInvokeAsyncAfter`, `nativeCancel`, `nativeReleaseOperation`, `nativeShutdown`, `nativeReleaseRuntime`, `nativeReleaseResult`, `nativeListSize`, `nativeListModelHandle`, `nativeRootModelHandle`, scalar result getters, error message/metadata/release, five `nativeModel*` field getters.

Also present, Kotlin-only (not JNI): `invokeAsyncAfterAwait` via `suspendCancellableCoroutine`.

`JNI_OnLoad` registers natives. Callbacks: `CrossaNativeCallback.onComplete`. Worker threads attach only for terminal delivery (android.md / bridge). Per-field JNI for scalars is the current Android model access pattern — architecture prefers path/bulk access; iOS already uses paths. Flagged as architectural performance risk, not a measured regression.

ABI path functions are **not** JNI-bound. That is the nested-view gap.

Dynamic `.so` exports: `JNI_OnLoad` plus **thousands of OpenSSL symbols** (`nm -D` showed 7758 `T` symbols on the example AAR). Hidden visibility / `--exclude-libs` is not applied. Binary-size and ABI-surface issue (P2).

---

## 18. Android AAR Packaging Status

**Status: Implemented** for arm64-v8a Debug and Release *generation*; example consumes Debug.

Command: `crossa generate-build android <project> --output <dir> [--ndk-version --gradle-version --kotlin-version]`

Generated project includes Gradle wrapper, `library` module, CMake, pinned OpenSSL/curl/CA, 16 KB linker flags, consumer R8 rules, Debug (JNI debug, unminified) and Release (R8, native Release).

`scripts/run-android-project-generator-tests.sh` **passed**. It asserts no `AndroidUnavailableCurlTransport.cpp` in the generated tree (production curl is required).

### Example AAR inspected

`android-example/app/libs/crossa-generated-debug.aar` (2.6 MB, 2026-09-06)

| Entry | Observation |
|---|---|
| `AndroidManifest.xml` | package `com.crossa.generated`, minSdk 23 |
| `classes.jar` | 48 KB; `api/PostRequests`, `api/PostsRepository`, `model/Post`, runtime `CrossaState`/`CrossaError`/`CrossaNativeList`/`CrossaNativeResult`/`CrossaOperation`/`CrossaRuntime`, internal JNI bridge |
| `jni/arm64-v8a/libcrossa_runtime.so` | 7.3 MB, ELF aarch64, stripped |
| Other ABIs | **none** (matches `AndroidBuildRequirements` `arm64-v8a` only) |
| `proguard.txt` | keeps public api/runtime + JNI bridge/callback/argument types |
| Duplicate `.so` | none |

ELF `PT_LOAD` alignment **16384** — 16 KB page-size requirement met.

### Release AAR

Not in the example repo. Older core build tree `build/android-final-v13` (2026-08-31) contains both:

| Variant | AAR | `.so` |
|---|---|---|
| debug | 2.6 MB | 6.8 MB, stripped |
| release | 2.5 MB | 6.4 MB, stripped |

Debug vs Release are distinct. This audit did not rebuild Release against current sources (no CMake on `PATH`; ANDROID_HOME points at a missing directory).

---

## 19. Android Example / E2E Status

**Path**

```text
android-example/crossa/*.cra
  → scripts/generate-crossa-aar.sh
      → Crossa/build-host/crossa generate-build android
      → generated Gradle assembleDebug
      → app/libs/crossa-generated-debug.aar
  → app implementation(files("libs/crossa-generated-debug.aar"))
  → PostsRepository.getPosts { CrossaState }
  → JNI → native runtime → JSONPlaceholder
```

**Consumption model:** local generated **debug AAR**, not Maven, not source module, not a GitHub Release artifact.

**Shortcuts:** hardcoded `statusCode = 200` and `responseHeaderCount = 0` in the Crossa scenario; eager `CrossaPost.toDomain()` copy of every field; Retrofit/Ktor clients constructed per iteration (Crossa runtime is reused). Comparison is **not a fair benchmark**. README itself says “not a formal benchmark” while still ranking a winner.

**This session:** did not re-run the emulator. README + screenshot + APK timestamp 2026-09-06 claim a successful Pixel 10 Pro emulator run (Crossa 5/5). Treat on-device behavior as **documented by the example repo, not re-verified here**.

Doctor in this environment: `ANDROID_HOME=/Users/yazantarifi/Library/Android/sdk` is **not a directory**. A vendored SDK exists at `Crossa/build/android-sdk` (NDK 28.1.13356709). The example script prefers `ANDROID_HOME` over the vendored SDK, so a naive regenerate would fail in this shell until `ANDROID_HOME` is corrected.

---

## 20. Swift Generator Status

**Status: Implemented** for XCFramework generation. **No** `crossa generate swift` CLI (pure-Swift analogue of `generate kotlin`).

`SwiftGenerator::generateProject`:

- Linked-project index
- Canonical models under `model/`
- Functions grouped by source unit as `public extension CrossaFunctions` in `api/<Source>.swift`
- Sync functions can be pure-translated
- `@Async` / `@AsyncAfter` emit thin `runtime.invokeAsync` / `invokeAsyncAfter` wrappers
- `@AsyncAfter` also emits `async throws` using `invokeAsyncAfterAwait`
- Nested model/list fields supported
- `Json` results not supported in Swift generation

Installed XCFramework Swift interface (simulator, Swift 6.2.3, iOS 13, **`-Onone` Debug**):

- `CrossaFunctions.fetchPosts` / `getPosts` → `CrossaState<CrossaList<Post>>` and `async throws -> CrossaList<Post>`
- `struct Post` with `userId`, `id`, `title`, `body`
- `CrossaList`, `CrossaError`, `CrossaOperation.cancel/release`, `CrossaRuntime`, `CrossaState.success/failed/cancelled`

No URLSession client is generated.

Roadmap Phase 10 “Swift generation remains unimplemented” refers to the **pure CLI backend**, not XCFramework Swift. Milestone A “generate Swift” is therefore only met inside `generate-build ios`, not as `generate swift`.

---

## 21. iOS Native Bridge Status

**Status: Implemented** in generated project sources + `src/bindings/ios/CrossaIosRuntimeBridge.cpp`.

```text
Generated Swift API
  → CrossaRuntime / CrossaOperation / CrossaNativeValue
  → C ABI (crossaInvokeAsyncAfter, path accessors)
  → NativeRuntime
```

`crossaIosCreateGeneratedRuntime` reconstructs the generated program.

Callbacks: `@escaping (CrossaState<T>) -> Void` on the **native delivery thread** (ios.md); example hops to `@MainActor`. Exactly-once is the ABI completion contract. Cancellation: `CrossaOperation.cancel()` → `crossaCancelOperation`.

Nested access: `CrossaNativeValue.child(field:)` / `element(index:)` appends `CrossaAbiPathSegment`s.

This audit did not run TSAN against the Swift callback path and did not launch the simulator app. Lifetime design matches ADR 0009 by inspection.

---

## 22. XCFramework / Swift Package Status

**Status: Implemented for local consumption. Partial for publication.**

Installed at `ios-example/CrossaBinary/Crossa.xcframework` (2026-09-06):

| Slice | Arch | Binary | dSYM |
|---|---|---|---|
| `ios-arm64` | arm64 device | Mach-O dylib 1.2 MB | present |
| `ios-arm64-simulator` | arm64 simulator | Mach-O dylib 1.2 MB | present |

`Info.plist` uses `xcodebuild -create-xcframework` metadata (`LibraryIdentifier`, `SupportedPlatformVariant = simulator`). Not lipo-merged. Headers + `Modules/Crossa.swiftmodule` + `module.modulemap` present.

The **installed example framework is Debug** (`-Onone` in the swiftinterface). ios.md says generate-build writes **both** debug and release under the output directory. Example install script copies whatever path the developer passes; README uses `debug/Crossa.xcframework`.

### SwiftPM

| Location | What it is |
|---|---|
| `ios-example/CrossaPackage/Package.swift` | Local `binaryTarget(path: "../CrossaBinary/Crossa.xcframework")`. No URL, no checksum |
| Generated `build/crossa-ios/project/Package.swift` | Local `Artifacts/Crossa.xcframework` |
| `Package.swift.release.template` | GitHub Release URL + checksum placeholders `<CROSSA_VERSION>` / `<CROSSA_XCFRAMEWORK_CHECKSUM>` |

Release template is **not published**. Artifact manifest exists in the generated iOS project:

```json
compilerVersion 0.1.0, languageVersion foundation, irVersion 1,
runtimeAbiVersion 1, runtimeVersion 0.1.0, deploymentTarget 13.0,
curl 8.12.1, linkedDeclarationCount 7
```

`generate-build ios` was **not re-executed** here: `crossa doctor` reports CMake missing on `PATH` (vendored Android CMake exists at `Crossa/build/android-sdk/cmake/3.22.1` but is not on PATH). Existing XCFramework is accepted as prior successful packaging evidence.

---

## 23. iOS Example / E2E Status

```text
ios-example/crossa/*.cra
  → (external) crossa generate-build ios
  → scripts/install-crossa-framework.sh <xcframework>
  → CrossaPackage binaryTarget
  → CrossaPostsRepository
      CrossaFunctions.fetchPosts(runtime:) { CrossaState }
  → XCFramework → ABI → native runtime
```

The repository does **not** compile Crossa C++. It does include an Alamofire comparison (`AlamofirePostsRepository` + `Decodable` DTOs).

Note: the generated API also has `getPosts`; the example calls `fetchPosts` (the request function) directly.

This session did not run `xcodebuild` for the example app. Treat UI integration as **Unverified here**, with strong artifact + source evidence that the wiring is complete.

---

## 24. CLI Status

**Status: Implemented** for the documented command set.

```text
crossa                         interactive wizard (TTY required; non-TTY prints help, exit 1)
crossa --help / -h
crossa --version               prints 0.1.0
crossa doctor
crossa [check|run|test] [--debug] <file.cra>
crossa <file.cra>              shortcut for run
crossa run [--project-root <dir>] <file.cra>
crossa generate kotlin [--debug] <file.cra> --output <dir>
crossa generate-build android  [ndk/gradle/kotlin version flags, --entry]
crossa generate-build ios      [--entry]
crossa --no-input <command>    non-interactive policy
```

`crossa version` (without `--`) is treated as a `.cra` path and errors — by design.

There is no `clean` command. Blueprint’s future `extract/inspect/diff/benchmark` commands are not current requirements.

Interactive mode exists (`InteractiveCli`) and maps to the same `Arguments` model, including Android vs iOS generate-build. It was not driven interactively in this non-TTY audit.

Host binary: `build-host/crossa` Mach-O arm64, reports `0.1.0`.

---

## 25. Doctor / Installation Status

### Doctor — Implemented

`crossa doctor` in this environment:

| Check | Result |
|---|---|
| CLI 0.1.0, installation path, embedded assets | pass |
| macOS arm64 | pass |
| ANDROID_HOME | **fail** (set to a non-existent `~/Library/Android/sdk`) |
| Android NDK 28.1+ | fail |
| CMake / Ninja | fail on PATH |
| Java 20.0.1 | pass |
| Xcode 26.2, iOS SDK, Swift, clang++ | pass |
| CMake for iOS libcurl | fail |
| Cache / temp writable | pass |

Errors print a `Fix:` section. Actionable.

Vendored toolchain **does** exist at `Crossa/build/android-sdk/{ndk/28.1.13356709,cmake/3.22.1}` and was used to produce `build-host`. Doctor does not search that tree unless `ANDROID_HOME` points there.

### Installation — Implemented in scripts, **not usable today**

`scripts/install/install.sh` downloads `crossa-vX-macos-arm64.tar.gz` or `linux-x86_64` from GitHub Releases, verifies `SHA256SUMS`, installs to `~/.crossa/bin/crossa`. macOS x86_64 and Linux arm64 are **explicitly refused**. Windows uses `install.ps1` → `windows-x86_64.zip`.

```text
GET https://github.com/crossa-script/Crossa/releases/latest  → 404
```

No tags, no releases, no checksums. A new developer **cannot** install Crossa without cloning and building.

---

## 26. CI Status

### Core repo

| Workflow | Trigger | Platform | What it does | Permissions | Release role |
|---|---|---|---|---|---|
| `ci.yml` “Crossa CI” | push, pull_request | `ubuntu-latest` | apt cmake/ninja/libcurl; `./test.sh` | `contents: read` | none |
| `release.yml` “Release Crossa” | `workflow_dispatch` version input | validate on ubuntu-24.04; build matrix macos-15 / ubuntu-24.04 / windows-2022 | Release CLI binaries; SHA256SUMS; provenance attestation; `gh release create` | workflow `contents: read`; publish job `contents: write`, `id-token: write`, `attestations: write` | CLI-only GitHub Release |

Actions are **pinned to commit SHAs** (`actions/checkout@11bd719…`, `upload-artifact@ea165f8…`, `download-artifact@d3f86a1…`, `attest-build-provenance@e8998f9…`). Least privilege on CI. No secrets printed. No Android AAR job. No iOS XCFramework job. No sanitizer job. No format/static-analysis job.

`test.sh` runs host unit tests and CLI fixtures. It does **not** invoke `scripts/run-kotlin-generator-tests.sh` or `scripts/run-android-project-generator-tests.sh`, despite `docs/development/testing.md` claiming CI runs the Kotlin generator suite. The former local-network harness is no longer invoked.

### Example repos

No `.github/workflows`.

---

## 27. Release / Distribution Status

| Path | Can it happen today? |
|---|---|
| Push/tag version | No tags exist; release is manual `workflow_dispatch` from `main` |
| CI build CLI binaries | Workflow exists, never evidenced by a Release |
| Android artifacts in Release | **No** |
| iOS XCFramework ZIP in Release | Template exists; **not in required asset list**; **not published** |
| Checksums | Workflow generates `SHA256SUMS` **if** a release runs |
| Install from Release | **No** (404) |

### Version axes (not collapsed — good)

| Axis | Current value | Source |
|---|---|---|
| CLI / compiler | `0.1.0` | `CrossaVersion.cpp` |
| Language | `"foundation"` | iOS artifact manifest |
| IR | `"1"` | manifest |
| Runtime ABI | `1` | `CROSSA_ABI_VERSION` |
| Runtime | `0.1.0` | manifest |
| SDK/AAR | unversioned local file | example `files()` dependency |

Hardcoded `0.1.0` is not derived from Git.

---

## 28. Existing Test Coverage

| Subsystem | Coverage | Notes |
|---|---|---|
| Lexer | language-tests `verifyLexerTokens` | pass |
| Parser | `verifyParserAst`, conditionals, import position | pass |
| Project linker | unit + CLI missing/ambiguous/cycle/order/execution | pass |
| Semantic | model, numeric types, failures | pass |
| IR | `verifyIrLowering` + CLI `--debug` | pass |
| Native execution | `test.cra`, conditionals, test-runner | pass |
| Scheduler | runtime-tests queued/executing/shutdown cancel | pass (silent ok) |
| Networking | local harness + opt-in JSONPlaceholder | harness **Broken**; JSONPlaceholder `test` **pass** |
| ABI | indirect via runtime-tests + generated bindings | no dedicated ABI suite |
| Kotlin generator | language-tests subset + golden script | script pass; **not in test.sh** |
| Swift generator | **none** | |
| JNI | **none** automated | 24/24 by inspection |
| iOS bridge | **none** automated | |
| CLI | CTest/test.sh fixtures | pass |
| Packaging | android generator script | pass; not in test.sh |
| Sanitizers | CMake options; old binaries in `build/` dated 2026-08-29 | **not re-run** |

`crossa_runtime_tests` exited 0 with no stdout (expected).

---

## 29. Native Safety / Sanitizer Status

CMake:

```text
CROSSA_ENABLE_ASAN  → -fsanitize=address,undefined
CROSSA_ENABLE_TSAN  → -fsanitize=thread
mutually exclusive
```

Stale binaries exist (`build/crossa-sanitized`, `crossa-runtime-tests-sanitized`, `crossa-runtime-tests-tsan`, Aug 29). This audit did not re-run sanitizers (would require CMake configure). **Unverified** against current `main`.

CI does not run sanitizers.

---

## 30. Benchmark / Performance Readiness

**Status: demo timings exist; formal performance infrastructure does not.**

- No `benchmarks/` tree, no native benchmark target, no allocation/JNI-crossing counters, no binary-size CI budget.
- Android and iOS examples time 5 uncached JSONPlaceholder GETs with a 2 s delay and display averages. Screenshots committed.
- Android README (2026-09-06, Pixel 10 Pro emulator): Crossa 179.40 ms vs Ktor 828.20 vs Retrofit 1024.20. Explicitly “not a formal benchmark.”
- Fairness problems: new Retrofit/Ktor clients per iteration vs reused Crossa runtime; Crossa scenario hardcodes HTTP 200; Crossa copies every model field to a domain data class (JNI per field × 4 × N posts); different headers; emulator not a physical ARM64 device (architecture §17).

**Do not claim Crossa is faster than Retrofit/Ktor/Alamofire based on this audit.** Those numbers are example-app observations, not an accepted performance result.

Architectural performance risks (unmeasured): generic JSON DOM before model construction; per-field JNI; Android example materialization; 7.3 MB `.so` with exported OpenSSL symbols; curl-easy serialized on the scheduler (not curl-multi).

---

## 31. Architecture Conformance

| # | Principle | Verdict |
|---|---|---|
| 1 | C++ owns the hot path | **Conformant** |
| 2 | Kotlin/Swift bindings remain thin | **Conformant** (examples have their own stacks by design) |
| 3 | One C++ `.cra` frontend | **Conformant** |
| 4 | Semantics resolved before generators | **Conformant** |
| 5 | IR platform-neutral | **Conformant** |
| 6 | Networking stays native | **Conformant** |
| 7 | Native response parsing | **Conformant** (DOM+schema, not generated direct) |
| 8 | Native models source of truth | **Partially Conformant** — native yes; Android nested views missing; Android example copies |
| 9 | Shared bounded scheduler | **Conformant** |
| 10 | Platforms do not create alternate schedulers | **Conformant** (coroutines/async await only bridge) |
| 11 | ABI small | **Conformant** |
| 12 | Exceptions do not cross ABI | **Conformant** |
| 13 | Ownership/lifetimes explicit | **Partially Conformant** — generally yes; `curl_global_init`; Android process-wide `object CrossaRuntime` |
| 14 | Cross-boundary calls minimized | **Partially Conformant** — iOS paths are coarse; Android is per scalar field |
| 15 | Large result duplication avoided | **Partially Conformant** — runtime avoids it; Android example does not |
| 16 | Generated output deterministic | **Conformant** (golden Kotlin tests) |
| 17 | Runtime modules independent | **Conformant** (only network module exists) |
| 18 | No mutable global runtime state | **Partially Conformant** — libcurl global init; ABI handle registry singleton |
| 19 | Queues/buffers/pools bounded | **Conformant** |
| 20 | Material changes have ADRs | **Conformant** for 0001–0009; missing optimizer/adapters ADRs because those are unimplemented |

libcurl global init vs “no process-global mutable state”: **Crossa-specific exception** forced by the dependency; should be documented as such rather than treated as a silent violation.

---

## 32. Documentation Drift and Contradictions

High-severity drift (do not treat as code bugs):

| ID | Sources | Interpretation |
|---|---|---|
| D1 | Blueprint: language syntax deferred / not a scripting engine. Foundation + architecture: CRA V0 exists and `crossa run` executes it | Blueprint is historical |
| D2 | Blueprint V1 is adapter-first. README/architecture today are `.cra`-first | Adapters planned, not shipped |
| D3 | `AGENTS.md` freeze-list forbade `if`, `imports`, extra HTTP methods | **Corrected** after this audit; V0 `if`/`import`/HTTP methods are listed as supported |
| D4 | `AGENTS.md`: “no committed executable build configuration” | **Corrected**; now points at `CMakeLists.txt` and `./test.sh` |
| D5 | `AGENTS.md` routes to `docs/architecture/*`, `docs/project/product.md`, etc. | Files do not exist |
| D6 | Roadmap Phase 7: model/list/async “future work” | **Corrected**; Phase 7 banner now says Implemented |
| D7 | Roadmap Phase 10: Swift unimplemented | **Corrected**; distinguishes pure Kotlin CLI from `generate-build ios` Swift |
| D8 | Roadmap Phase 12 + networking.md: platform completion bridges “planned” | **Corrected**; banners now say generated Android/iOS bridges are implemented |
| D9 | Roadmap Phase 18: auth/multipart/retry planned | **Corrected**; those request properties are listed as implemented |
| D10 | Architecture §15: C ABI vs Swift interop “open” | **Corrected**; now cites ADR 0009 C ABI |
| D11 | Architecture §18: simdjson candidate | ADR 0001: Crossa-owned bounded parser |
| D12 | Foundation §8/§38 omit `Long`/`Double`; architecture and generators include them | Spec hole |
| D13 | testing.md: CI runs Kotlin generator suite | `test.sh` / `ci.yml` do not call `run-kotlin-generator-tests.sh` |
| D14 | testing.md “two test layers” then lists four | Internal inconsistency |
| D15 | android.md “while the native runtime embedding work is completed” | **Corrected**; embedding is described as included |
| D16 | Language-foundation source-unit → `UsersController.swift`. iOS emits `extension CrossaFunctions` | ios.md wins for iOS API shape |
| D17 | README advertises install-from-release | Releases 404 |
| D18 | Foundation §71 listed headers/body/extra CLI as “do not invent” | **Corrected**; those implemented items were removed from the open-decision list |

Fuller conflict catalog from the authority-document pass is omitted here only where it repeats the above. Blueprint Phase 11/§12 V1 networking (HTTP/2, nullability) is **future**, not a current defect.

---

## 33. Technical Debt / Stubs / TODOs

Meaningful product debt (not an indiscriminate comment dump):

| Item | Where | Notes |
|---|---|---|
| `AndroidUnavailableCurlTransport.cpp` | `src/bindings/android/` | Placeholder transport for non-curl embeddings; **must not** appear in generated Android projects (generator test enforces this) |
| No IR optimizer | architecture diagram vs tree | Planned |
| No backend adapters | architecture diagram vs tree | Planned |
| No `generate swift` CLI | `CrossaApplication` | Swift only via `generate-build ios` |
| Hardcoded `0.1.0` | `CrossaVersion.cpp` | Not Git-derived |
| OpenSSL symbol export | Android `.so` | 7758 global `T` symbols |
| Android nested model/list fields | `KotlinGenerator::emitNativeModel` | Explicit `failUnsupported` |
| Android `List<scalar>` / `Json` results | `emitNativeFunction` | Explicit `failUnsupported` |
| Local-network curl URL assertion | `scripts/run-local-network-tests.sh` | Removed from the suite |
| Kotlin/Android generator scripts off `test.sh` | testing.md vs test.sh | Doc/CI gap |
| Debug XCFramework in ios-example | swiftinterface `-Onone` | Install path chooses debug |
| Android example eager `toDomain()` | `NetworkComparisonRepository.kt` | Architecture anti-pattern in the demo |
| `curl_global_init` | `CurlTransport.cpp` | Process-global |
| Source UTF-8 well-formedness | `SourceLoader` | Not validated |
| No JNI/iOS automated tests | tests/ | Gap |

`TODO`/`FIXME`/`HACK` hits in production C++ were essentially absent; the codebase is not littered with placeholders.

---

## 34. Roadmap Traceability Matrix

| Phase | Goal | Claimed | Actual | Blocking? |
|---|---|---|---|---|
| 0 Language contract | Foundation doc | implied | Implemented | no |
| 1 Source/diagnostics | SourceFile, limits | unlabeled | Implemented | no |
| 2 Lexer | tokenize V0 | unlabeled | Implemented | no |
| 3 Parser/AST | V0 AST | unlabeled | Implemented | no |
| 4 Types | Int/String/Bool/List | unlabeled; omits Json/Long/Double | Implemented including Json/Long/Double | no |
| 5 Semantic | typed meaning | **Implemented** | Implemented | no |
| 6 IR | platform-neutral IR | **Implemented** | Implemented | no |
| 7 Native scalar exec | interpreter | Partial; models/async “future” | **Stale claim** — interpreter + models + scheduler exist | no |
| 8 Interpolation | `#id` | unlabeled | Implemented | no |
| 9 Models | native/Kotlin/Swift | unlabeled | Native+both platforms (Android nested fields incomplete) | no |
| 10 Pure generators | Kotlin+Swift | Kotlin yes, Swift no | Kotlin CLI yes; Swift via XCFramework only | no for mobile milestone |
| 11 Scheduler policies | @Sync/@Async/@AsyncAfter | unlabeled | Implemented | no |
| 12 Completion state | platform bridges | native yes, bridges “planned” | **Stale** — bridges exist | no |
| 13 Config | networking keys | subset implemented | Full foundation set in code | no |
| 14 CrossaRequest | libcurl | Implemented | Implemented | no |
| 15 Request interpolation | plans | Implemented | Implemented | no |
| 16 Decoding | DOM+schema; direct later | accurate | Matches | no |
| 17 Generated APIs | thin Kotlin/Swift | unlabeled | Implemented | no |
| 18 Extra request fields | auth/multipart/retry planned | **Stale** | Implemented in engine | no |
| 19 Project CLI | imports + Android gen | Implemented | Implemented; iOS too | no |
| 20 Performance | measure after pipeline | unlabeled | Demo only | no |
| 21 Growth gate | future syntax | gate | Not started (correct) | no |

**Milestones A–D**

| Milestone | Required | Actual |
|---|---|---|
| A Pure function parse/validate/lower/exec + Kotlin + Swift | Swift CLI not present | Native exec + Kotlin CLI + Swift inside XCFramework |
| B Interpolation without runtime rescan | yes | Implemented |
| C `List<User>` as typed collection | yes | Implemented |
| D `@AsyncAfter` Success/Failed/Cancelled on Android/iOS | yes | Generated APIs exist; Android/iOS examples wired |

---

## 35. Completion Scores

Capability scores (0.00 absent … 1.00 closed and verified):

| Capability | Score | Why not 1.00 |
|---|---|---|
| Language/frontend | 0.92 | UTF-8 well-formedness; foundation Long/Double hole |
| Compiler/semantic/IR | 0.88 | No optimizer; IR “link” is generator-side concatenation |
| Native execution/runtime | 0.86 | curl global init; sanitizers not re-run |
| Networking | 0.80 | Local harness broken; functional decoder not direct; curl-easy not multi (accepted) |
| Android generation | 0.86 | Nested fields / List-of-scalar / Json unsupported |
| Android JNI/native views | 0.74 | No path JNI; per-field crossings |
| Android packaging/example | 0.82 | Example is debug-only; on-device run not re-verified; no Maven |
| Swift generation | 0.84 | No `generate swift` CLI; no golden tests; Json unsupported |
| iOS bridge/native views | 0.84 | Unverified live app this session; Debug framework in example |
| iOS packaging/example | 0.78 | Local SPM only; release ZIP unpublished; CMake missing in this env |
| CLI/tooling | 0.88 | Interactive not TTY-tested; version hardcoded |
| CI/release/distribution | 0.32 | No releases; no mobile artifacts in release; likely red local-network on CI |
| Testing/native safety | 0.68 | Harness fail; generator scripts off default suite; no JNI/iOS tests; sanitizers stale |
| Performance readiness | 0.30 | Demo only; unfair comparison |
| Documentation consistency | 0.42 | See §32 |

### Roll-ups

Weighting for **current compiler/mobile milestone** (not future modules, not public distribution):

```text
Core compiler = 0.40 * (frontend + semantic/IR + native + networking) / 4
              = 0.40 * (0.92+0.88+0.86+0.80)/4
              = 0.346

Android       = 0.30 * (gen + JNI + packaging) / 3
              = 0.30 * (0.86+0.74+0.82)/3
              = 0.242

iOS           = 0.30 * (swift + bridge + packaging) / 3
              = 0.30 * (0.84+0.84+0.78)/3
              = 0.246

Current-milestone readiness = 83%  → report 81% after applying
the failing local-network quality-gate penalty (−2 pts).
```

| Roll-up | % | Weighting note |
|---|---|---|
| **Core compiler readiness** | **86%** | frontend/IR/runtime/network, no distribution |
| **Android readiness** | **80%** | generation + JNI + example AAR |
| **iOS readiness** | **82%** | Swift + bridge + XCFramework |
| **Distribution readiness** | **32%** | CLI install 0.25 (scripts exist, releases missing); Android independent AAR 0.50 (file works, unpublished); iOS SPM URL 0.25; CI release 0.30 |
| **Overall current-milestone readiness** | **81%** | compiler/mobile functional closure; quality-gate failure included |

Scaffolds were not scored as complete.

---

## 36. P0 Blocking Gaps

### P0-1 — Advertised CLI installation from GitHub Releases does not work

- **Repository:** Crossa  
- **Subsystem:** Distribution  
- **Current state:** Planned/unimplemented at the publication layer (scripts exist)  
- **Evidence:** no tags; `https://github.com/crossa-script/Crossa/releases/latest` → 404; README and `docs/development/release.md` instruct `curl … install.sh`.  
- **Why incomplete:** The documented onboarding path cannot succeed.  
- **Impact:** New developers must clone and build. Example repos already assume a local `build-host/crossa`.  
- **Dependencies:** green `test.sh`; `release.yml` on `main`  
- **Next objective:** Run the existing Release workflow for `0.1.0` and verify `install.sh` on macOS ARM64.

This is the remaining P0. Nested Android views, SPM publication, and generator-test CI inclusion are P1.

---

## 37. P1 Required Before Production V1

### P1-1 — Android nested model and list fields

- **Repo:** Crossa (`KotlinGenerator::emitNativeModel`, `AndroidJniBridge`)  
- **State:** Explicit `failUnsupported`  
- **Evidence:** generator switch rejects `SemanticTypeKind::Model`/`List` fields; JNI has no `crossaGetPath*` bindings; iOS already emits path accessors.  
- **Impact:** Any V0 model with nested models/lists cannot generate Android. Parity break.  
- **Objective:** Bind ABI path getters in JNI; emit Kotlin nested views equivalent to Swift `nativeValue.child(field:)`.

### P1-2 — Put Kotlin and Android generator tests on the default suite

- **Repo:** Crossa (`test.sh`, `ci.yml`, `docs/development/testing.md`)  
- **State:** Scripts pass when run manually; CI does not run them  
- **Objective:** Invoke `run-kotlin-generator-tests.sh` and `run-android-project-generator-tests.sh` from `test.sh`.

### P1-3 — Swift generator golden tests

- **Repo:** Crossa  
- **State:** No Swift equivalent of `tests/kotlin-generator*`  
- **Objective:** Snapshot `generate-build ios` Swift outputs (or a unit wrapper around `SwiftGenerator`) for models, Sync, AsyncAfter, nested fields.

### P1-4 — Example artifacts should be Release (or explicitly dual)

- **Repos:** android-example, ios-example  
- **State:** Debug AAR; Debug `-Onone` XCFramework  
- **Objective:** Document and consume Release AAR / `release/Crossa.xcframework` for the public examples.

### P1-5 — Independent iOS consumption via checksummed binaryTarget

- **Repo:** Crossa packaging + ios-example  
- **State:** `Package.swift.release.template` unpublished; example uses path binaryTarget  
- **Objective:** Publish `Crossa.xcframework.zip` + checksum as the template describes.

### P1-6 — Android `List<scalar>` and `Json` result mapping

- **Repo:** Crossa Kotlin generator  
- **State:** `failUnsupported("native-backed Android result type")`  
- **Objective:** Map those result kinds through existing ABI scalar/list accessors.

### P1-7 — JNI / iOS callback automated tests

- **Repo:** Crossa  
- **State:** No tests  
- **Objective:** Host-side ABI tests already exist; add JNI registration tests and a Swift package test or generated-project compile test for exactly-once completion and cancel.

### P1-8 — Doctor / ANDROID_HOME footgun

- **Repo:** Crossa doctor + android-example script  
- **State:** A set-but-invalid `ANDROID_HOME` hides the vendored SDK  
- **Objective:** Treat non-directory `ANDROID_HOME` as unset, or search the Crossa cache.

### P1-9 — Foundation spec: `Long` / `Double` / token catalogs

- **Repo:** Crossa docs  
- **State:** Code implements; foundation grammar incomplete  
- **Objective:** Update `language-foundation.md` (not a new roadmap) so V0 types match `SemanticType`.

---

## 38. P2 Hardening Work

- Hidden visibility / `--exclude-libs` for Android OpenSSL/curl so `.so` does not export 7k+ crypto symbols.
- UTF-8 well-formedness in `SourceLoader`.
- Re-enable ASan/UBSan (and TSan where compatible) in CI.
- Stop Android example from eagerly copying every native field in the comparison path (or mark the copy as explicit materialization).
- Fair benchmark harness on physical ARM64 devices (after functional closure).
- Derive CLI version from Git describe / release input instead of a string literal.
- Document `curl_global_init` as an accepted dependency exception.
- Refresh stale roadmap status banners (Phase 7/10/12/18) and AGENTS.md freeze-list so agents stop fighting the code.
- `check` vs `config.cra`: make CLI help state that `check` skips config (foundation already says so).
- Windows release job: never proven; treat as Unverified until a release runs.

---

## 39. P3 Future Roadmap

Do **not** start these as part of the current milestone:

- Backend adapters / NestJS contract extraction  
- IR optimizer  
- Generated direct schema decoders (explicit later optimization)  
- curl-multi / HTTP/2 as default transport  
- simdjson  
- Streaming ABI to Kotlin/Swift  
- Database, WebSocket, sockets, cache, crypto  
- Language growth: loops, switch, nullables, enums, maps, packages, generics  
- Extra CPU ABIs (armeabi-v7a, x86_64 Android, sim x86_64)  
- macOS x86_64 / Linux ARM64 CLI archives  

---

## 40. Android Readiness Verdict

**READY WITH NON-BLOCKING GAPS**

Closed for the current demo graph:

```text
multi-file .cra → compiler → IR → generated native program
  → runtime → generated Kotlin → JNI (24/24) → NativeList<Post>
  → arm64-v8a debug AAR (16 KB aligned) → android-example files() dependency
```

Gaps that keep it from unqualified READY:

- Nested model/list fields cannot generate.
- Example consumes debug AAR only; Maven/publication absent.
- Example materializes domain copies.
- On-device run not re-executed in this audit (APK/AAR/README dated today).
- No example CI.
- Invalid `ANDROID_HOME` in this environment would block regeneration.

A generator, JNI layer, or AAR script **alone** would not have been enough. The chain exists.

---

## 41. iOS Readiness Verdict

**READY WITH NON-BLOCKING GAPS**

Closed for the current demo graph:

```text
multi-file .cra → compiler → IR → generated native program
  → runtime → generated Swift CrossaFunctions
  → C ABI + CrossaNativeValue paths → CrossaList<Post>
  → XCFramework ios-arm64 + ios-arm64-simulator
  → local SwiftPM binaryTarget → SwiftUI example
```

Gaps:

- Example installs Debug (`-Onone`) XCFramework.
- SPM is path-based, not the release URL+checksum template.
- No `generate swift` CLI; no Swift golden tests.
- `generate-build ios` not re-run here (CMake missing on PATH).
- Simulator app not launched in this audit.
- No example CI.
- iOS has `async throws` **and** completion closures; Android has callback + `suspend`. Parity of *semantics* is good; API shape differs (instance `PostsRepository` vs `CrossaFunctions`).

---

## 42. Compiler/Language Readiness Verdict

**READY WITH NON-BLOCKING GAPS**

V0 surface defined by `language-foundation.md` is present in lexer/parser/semantic/IR/runtime, with these caveats:

- `Long`/`Double` implemented but underspecified in the foundation grammar.
- Nested Android views lag native/iOS.
- `assert` only in `crossa test` (specified).
- `check` skips `config.cra` (specified).
- No optimizer (not a V0 language requirement).

Unsupported future syntax was **not** scored as missing.

Host proof: language tests pass; import graph check; pure `add`; conditionals; live JSONPlaceholder `Post` decode.

---

## 43. Distribution Readiness Verdict

**NOT READY**

| Channel | Verdict |
|---|---|
| CLI without cloning | **No** — Releases 404 |
| Another Android app consuming a generated AAR independently | **Yes, locally** — `files("…aar")` works; not a published coordinate |
| Another iOS app consuming XCFramework/SPM independently | **Yes, locally** — path binaryTarget works; GitHub URL template unpublished |
| CI versioned artifacts | **Workflow exists, never produced a visible Release** |

---

## 44. Recommended Next Phase

**Close the existing quality gate and Android nested-view parity — do not start a new product surface.**

Why this is next:

1. The compiler/mobile vertical slice already runs. Building adapters, WebSockets, or a benchmark product would inflate scope while `test.sh` is red and Android cannot express nested models that iOS already can.
2. Documentation already *claims* generator tests run in CI and that install-from-release works. Those claims are currently false. Fixing them restores an honest project state.
3. Nested Android views are the only remaining hole on the architecture’s “native-backed models are the source of truth” rule for the V0 type system.

**Blockers that precede feature work:** P0-1 (GitHub Release) should follow a green suite.

**Repositories:** Crossa first; android-example/ios-example only when swapping debug→release artifacts.

**Done means:**

- `./test.sh` exits 0 on a machine with CMake or a C++20 toolchain.
- Kotlin + Android generator scripts are on that path.
- A `.cra` model with a nested model field generates Android Kotlin that reads through JNI path accessors without copying the graph.
- Optional but recommended: `v0.1.0` GitHub Release installable via `install.sh`.

**Explicitly out of scope for this phase:** adapters, new `.cra` syntax, curl-multi, simdjson, direct decoders, performance claims, extra ABIs.

---

## 45. Exact Next Implementation Sequence

1. **Fold generator tests into `test.sh`** — `run-kotlin-generator-tests.sh` and `run-android-project-generator-tests.sh` (P1-2). Confirm `ci.yml` still just calls `test.sh`.
2. **Android nested views (P1-1)** — JNI-bind `crossaGetPath*` / list-size-at-path; extend `emitNativeModel` for Model and List fields; add a fixture model with a nested model + nested list; extend Android generator tests.
3. **Android List-of-scalar and Json results (P1-6)** if they fall out of the same mapper work; otherwise immediately after.
4. **Swift golden tests (P1-3)** so iOS generation cannot regress while Android catches up.
5. **Point examples at Release artifacts (P1-4)** — debug remains available for JNI debugging.
6. **Publish CLI `v0.1.0` (P0-1)** using existing `release.yml`; verify `install.sh` on macOS ARM64.
7. **Only then** publish XCFramework ZIP + checksum (P1-5).
8. **Only after functional closure** — physical ARM64 benchmarks with a fair harness. Do not treat example-app averages as product claims.

---

## 46. Validation Commands Executed

| Repository | Command | Exit | Interpretation |
|---|---|---|---|
| Crossa | `build-host/crossa --version` | 0 | prints `0.1.0` |
| Crossa | `build-host/crossa --help` | 0 | command surface matches cli.md |
| Crossa | `build-host/crossa doctor` | ≠0 | missing ANDROID_HOME dir, NDK, CMake, Ninja |
| Crossa | `build-host/crossa_language_tests` | 0 | “Crossa language tests passed” |
| Crossa | `build-host/crossa_runtime_tests` | 0 | silent pass |
| Crossa | `crossa test.cra` | 0 | printed `3` |
| Crossa | `crossa check examples/imports/runPosts.cra` | 0 | 4 modules / 5 declarations |
| Crossa | `crossa run tests/import-project/entry/runImports.cra` | 0 | printed `9` |
| Crossa | `crossa test tests/test-runner/pass.cra` | 0 | |
| Crossa | `crossa test tests/conditionals.cra` | 0 | |
| Crossa | `crossa tests/import-project/entry/runDiamondImports.cra` | 0 | printed `10` |
| Crossa | `crossa examples/imports/repositories/postsRepository.cra` | 0 | |
| Crossa | `crossa tests/all-http-methods.cra` | 0 | |
| Crossa | `crossa tests/config.cra` | 0 | |
| Crossa | missing/ambiguous/cycle/execution/order import failures | 1, expected text | pass |
| Crossa | `crossa test tests/test-runner/failure.cra` | 1, assertion text | pass |
| Crossa | `scripts/run-local-network-tests.sh` | **1** | functional requests succeeded; curl URL assertion failed |
| Crossa | `scripts/run-kotlin-generator-tests.sh` | 0 | “Kotlin generator CLI tests passed” |
| Crossa | `scripts/run-android-project-generator-tests.sh` | 0 | “Android project generator tests passed” |
| Crossa | `crossa generate kotlin tests/kotlin-generator/Math.cra` | 0 | wrote `Math.kt` |
| Crossa | `crossa generate kotlin` invalid request | 1 | rejects CrossaRequest |
| Crossa | `crossa test tests/network-jsonplaceholder.cra` | 0 | live Post decode |
| Crossa | `crossa test examples/imports/runPosts.cra` | 1 | live List<Post> printed; no assert in example |
| Crossa | `curl` GitHub Releases latest | 404 | no releases |
| android-example | unzip/inspect `crossa-generated-debug.aar` | n/a | contents documented in §18 |
| android-example | ELF 16 KB alignment python check | n/a | `PT_LOAD align=16384` |
| ios-example | `plutil`/`lipo`/`file` on XCFramework | n/a | two arm64 slices, dSYMs |
| — | `cmake` on PATH | missing | host rebuild not attempted; used `build-host` |
| — | Android emulator / `xcodebuild` example | **not run** | Unverified UI |
| — | ASan/TSan current sources | **not run** | stale binaries only |
| — | `release.yml` | **not run** | would publish; out of audit policy |

### Could not be executed (environment)

- `cmake` / `ninja` not on `PATH` (vendored CMake exists under `build/android-sdk/cmake/3.22.1` but was not used to reconfigure).
- `ANDROID_HOME` set to a non-existent directory; NDK not visible to doctor.
- `socat` missing (ruby local server was used instead).
- GitHub Releases (none).
- Example app UI (emulator/simulator).
- Interactive CLI (non-TTY).
- Sanitizer rebuilds.

---

## 47. Evidence Appendix

### Repository HEADs

```text
Crossa           1d499bf0cdfe5564c521f3511e2ae4b186a358fb  main  clean
android-example  13fa938acec1fc501040b71c0c4a867651e57af4  main  clean
ios-example      33b18be47a552594087f4d69c9143e5c4f599eaf  main  clean
```

### Key symbols

| Area | File | Symbol |
|---|---|---|
| CLI | `src/cli/CrossaApplication.cpp` | `CrossaApplication::run` |
| Version | `src/cli/CrossaVersion.cpp` | `CrossaVersion::current` → `"0.1.0"` |
| Loader | `src/compiler/source/SourceLoader.cpp` | `SourceLoader::load` |
| Lexer | `src/compiler/lexer/Lexer.cpp` | `Lexer::tokenize` |
| Parser | `src/compiler/parser/Parser.cpp` | `Parser::parse` |
| Linker | `src/compiler/project/ProjectLinker.cpp` | `ProjectLinker::link` |
| Semantic | `src/compiler/semantic/SemanticAnalyzer.cpp` | `SemanticAnalyzer::analyze` |
| IR | `src/compiler/ir/IrLowerer.cpp` | lowering pass |
| Native gen | `src/compiler/generators/native/NativeProgramGenerator.cpp` | `generateProgramSource` |
| Kotlin | `src/compiler/generators/kotlin/KotlinGenerator.cpp` | `generateProject`, `emitNativeModel` |
| Swift | `src/compiler/generators/swift/SwiftGenerator.cpp` | `SwiftGenerationSupport::generate` |
| Interpreter | `src/runtime/IrInterpreter.cpp` | `evaluateRequest` |
| Runtime | `src/runtime/NativeRuntime.cpp` | `invokeAsyncAfter` |
| Scheduler | `src/runtime/scheduler/TaskScheduler.cpp` | `submitWithCompletion` |
| Network | `src/network/NetworkEngine.cpp` | retry/auth/coalesce |
| Transport | `src/network/transport/CurlTransport.cpp` | pool + mime |
| Decoder | `src/network/response/ResponseDecoder.cpp` | DOM → native |
| ABI | `include/crossa/bindings/shared-abi/CrossaAbi.h` | `CROSSA_ABI_VERSION 1` |
| JNI | `src/bindings/android/AndroidJniBridge.cpp` | `JNI_OnLoad` / `RegisterNatives` |
| iOS ABI glue | `src/bindings/ios/CrossaIosRuntimeBridge.cpp` | `crossaIosCreateGeneratedRuntime` |
| Android pack | `src/packaging/android/AndroidProjectGenerator.cpp` | Gradle + JNI Kotlin sources |
| iOS pack | `src/packaging/ios/IosProjectGenerator.cpp` | `xcodebuild archive` + `-create-xcframework` |

### Android vs iOS parity matrix

| Capability | Android | iOS | Gap |
|---|---|---|---|
| Generated models | yes, scalar fields | yes, nested too | Android nested |
| Generated functions | source-unit classes | `CrossaFunctions` extensions | API shape |
| Sync | pure or native | pure or native | none material |
| Async | `CrossaOperation` | `CrossaOperation` | none |
| AsyncAfter callback | `CrossaState` | `CrossaState` | none |
| AsyncAfter structured concurrency | `suspend` | `async throws` | idiomatic difference |
| CrossaState | sealed interface | enum | none semantic |
| CrossaError | class | struct Error | none semantic |
| Cancellation | operation handle | operation handle | none |
| Runtime init | `object CrossaRuntime.configure` singleton | `CrossaRuntime()` instance | ownership model |
| Runtime release | `close()` | `close()`/`shutdown()` | none |
| Model handles | JNI handle + owner | `CrossaNativeValue` path | Android no nested |
| List handles | `CrossaNativeList` of models | `CrossaList` | Android list-of-scalar missing |
| Nested values | **no** | **yes** | **P1** |
| Networking | native | native | none |
| Config | `config.cra` + overrides | `config.cra` | Android-only typed overrides object |
| JNI / bridge | JNI 24 methods | C ABI + Swift | path APIs JNI-missing |
| Artifact | AAR arm64-v8a | XCFramework 2 slices | extra Android ABIs N/A |
| Debug artifact | example uses | example uses | both debug |
| Release artifact | generator supports; example unused | generator supports; example unused | P1 |
| Example integration | files(AAR) | local SPM | neither published |
| Standalone consumption | local AAR yes | local XCFramework yes | no registry |
| CI build | core generates tests, not the example | none | P2 |
| Release publishing | no | template only | P0 distribution |

### Compiler vs platform ownership

| Concern | Expected owner | Actual owner | Conformant? |
|---|---|---|---|
| `.cra` lexing | C++ | C++ `Lexer` | yes |
| `.cra` parsing | C++ | C++ `Parser` | yes |
| Symbol resolution | C++ semantic | `SemanticAnalyzer` | yes |
| Type checking | C++ semantic | `SemanticAnalyzer` | yes |
| Interpolation | C++ semantic/IR | IR string build | yes |
| Request plan | C++ IR | `IrCrossaRequestExpression` | yes |
| Network execution | C++ network | `NetworkEngine`/`CurlTransport` | yes |
| Response parsing | C++ decoder | `ResponseDecoder` | yes |
| Model storage | C++ runtime | `NativeModel` | yes |
| List storage | C++ runtime | `NativeList` | yes |
| Async scheduling | C++ scheduler | `TaskScheduler` | yes |
| Error creation | C++ runtime | `CrossaError` | yes |
| Cancellation | C++ + thin bridge | handle + JNI/Swift cancel | yes |
| Kotlin API shape | Kotlin generator | `KotlinGenerator` | yes |
| Swift API shape | Swift generator | `SwiftGenerator` | yes |
| AAR packaging | Android packager | `AndroidProjectGenerator` | yes |
| XCFramework packaging | iOS packager | `IosProjectGenerator` | yes |

### What should NOT be worked on yet

Adapters, WebSocket/database, new `.cra` syntax, curl-multi cutover, simdjson, generated direct decoders as a rewrite, extra CPU ABIs, “make Crossa faster than Retrofit” programs, rewriting AGENTS.md freeze-list *instead of* fixing P0-1 (docs can land with P1-9, not as the next engineering phase).

---

*End of audit. This file is the durable source of the 2026-09-06 Crossa ecosystem status.*
