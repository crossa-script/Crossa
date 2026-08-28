# Crossa — Project Blueprint, Engineering Instructions, Architecture, and Roadmap

> **Document status:** Foundational project specification  
> **Primary audience:** Crossa maintainers, compiler/runtime engineers, Android/iOS integration engineers, CI/release engineers, and coding agents  
> **Primary implementation language:** Modern C++  
> **Initial target platforms:** Android and iOS  
> **Initial product domain:** Compiled, generated, high-performance API SDKs  
> **Long-term platform:** Extensible native runtime for networking, databases, WebSockets, raw/binary sockets, streaming, storage, and additional high-performance services

---

## 1. Executive Summary

Crossa is a **native SDK compiler and portable runtime platform** designed to move expensive, repetitive, and error-prone client infrastructure out of Android and iOS application code and into a shared, highly optimized C++ engine.

The first production capability of Crossa will be networking.

A backend project will be inspected or integrated through a backend adapter. The backend contract will be converted into Crossa's framework-independent internal contract and intermediate representation. Crossa will then generate and compile a complete native client SDK containing:

- API operations.
- Request construction.
- Path/query/header/body encoding.
- Authentication metadata and request policies.
- Response parsing.
- Typed native response objects.
- Error mapping.
- Runtime metadata.
- Platform bindings.
- Android packaging.
- iOS packaging.

The Android application should consume a generated **AAR**.  
The iOS application should consume a generated **XCFramework** and, where appropriate, a Swift Package wrapper.

The application developer must not need to manually implement networking DTOs, serializers, API endpoint definitions, request builders, JSON parsers, duplicated error mapping, or repeated platform-specific networking infrastructure.

The most important architectural rule is:

> **The hot path must remain native.**

For networking, request generation, request serialization, transport execution, response buffering, parsing, native model creation, memory management, scheduling, and protocol handling must be owned by the C++ runtime. Kotlin and Swift are integration surfaces, not secondary networking runtimes.

Crossa is not intended to become a reimplementation of Kotlin, Swift, Ktor, Retrofit, OkHttp, or URLSession. Its value is the **compiler-driven end-to-end pipeline** that turns backend contracts into optimized native SDKs.

The initial product must prove that architecture before Crossa is expanded into additional engines such as:

- Database.
- WebSocket.
- Raw socket.
- Binary socket.
- Streaming.
- Cache.
- File transport.
- Compression.
- Cryptographic services.
- Telemetry.
- Additional protocol/runtime modules.

The Crossa language syntax is intentionally **out of scope for this document**. Syntax will be designed after the contract model, compiler architecture, type system requirements, ABI, runtime ownership model, and platform ergonomics are stable.

---

# 2. Product Vision

Crossa should become a **portable compiled application-runtime platform** whose core behavior is implemented once in C++ and exposed through ergonomic platform APIs.

The long-term vision is:

```text
Backend / Contract Sources
            |
            v
     Crossa Extraction
            |
            v
      Crossa Contract
            |
            v
       Crossa Compiler
            |
            v
         Crossa IR
            |
       Optimization
            |
      Native Generation
            |
            v
  Crossa C++ Runtime + Modules
       /               \
      /                 \
 Android                 iOS
   AAR                XCFramework
```

Crossa must optimize for:

1. **Maximum useful work in native code.**
2. **Minimum platform-boundary crossings.**
3. **Minimum object materialization.**
4. **Minimum heap allocation.**
5. **Minimum memory copying.**
6. **Compile-time generation instead of runtime discovery.**
7. **Stable and versioned binary interfaces.**
8. **Deterministic behavior across Android and iOS.**
9. **Incremental compilation and modular packaging.**
10. **Extensibility without rewriting the runtime.**

The project should be built as a platform, not as one networking library.

---

# 3. Product Definition

## 3.1 What Crossa Is

Crossa is composed of several cooperating systems:

### A. Backend Adapters

Framework-specific adapters inspect backend definitions and produce a normalized contract.

Examples of future adapters:

- NestJS.
- Spring Boot.
- Express/Fastify.
- OpenAPI.
- Smithy.
- Protobuf/gRPC descriptors.
- Other contract sources.

Backend adapters are inputs to Crossa. They are not part of the runtime.

### B. Crossa Contract Model

The contract model is a framework-neutral representation of:

- Services.
- Operations.
- Models.
- Fields.
- Types.
- Nullability.
- Validation constraints.
- HTTP metadata.
- Headers.
- Query parameters.
- Path parameters.
- Bodies.
- Authentication requirements.
- Response variants.
- Error contracts.
- Streaming metadata.
- Version metadata.
- Feature metadata.

The contract model must not contain NestJS-, Spring-, Kotlin-, Swift-, or platform-specific assumptions.

### C. Crossa Compiler

The compiler validates the contract, performs semantic analysis, creates Crossa IR, applies optimizations, links selected modules, generates native execution code, and generates platform bindings.

### D. Crossa Runtime

The C++ runtime owns performance-critical runtime concerns:

- Memory.
- Scheduling.
- Networking.
- Transport state.
- Buffers.
- Serialization.
- Deserialization.
- Error representation.
- Native object lifetime.
- Module lifecycle.
- Logging hooks.
- Observability hooks.
- Resource pools.

### E. Crossa Modules

Networking is the first module.

Future modules can include:

- Database.
- WebSocket.
- Socket.
- Binary socket.
- Streaming.
- Files.
- Cache.
- Compression.
- Crypto.
- Telemetry.

Modules must plug into the runtime rather than creating independent runtimes.

### F. Platform Bindings

Android and iOS expose platform-idiomatic APIs while avoiding duplication of the underlying work.

Android:

- Kotlin-facing API.
- JNI boundary.
- AAR packaging.
- Native shared libraries.

iOS:

- Swift-facing API.
- Stable native boundary.
- XCFramework packaging.
- Optional Swift Package distribution wrapper.

---

# 4. What Crossa Is Not

Crossa must explicitly reject scope that weakens its differentiation.

Crossa is not:

- A Kotlin Multiplatform replacement.
- A UI framework.
- A general application framework.
- A JavaScript runtime.
- A generic scripting engine.
- A Retrofit clone.
- A Ktor clone.
- An OkHttp clone.
- A URLSession clone.
- A generic JSON library.
- A database ORM in V1.
- A DSL whose syntax is designed before runtime requirements are understood.
- A collection of platform implementations with a shared logo.

The networking engine may use proven low-level libraries internally. Crossa's value is not in manually rewriting TLS, HTTP/2, compression, cryptography, or every protocol primitive.

---

# 5. Non-Negotiable Engineering Principles

## 5.1 C++ Owns the Hot Path

For every performance-critical feature, the dominant execution path should remain in C++.

For networking this includes:

- Request validation.
- Request planning.
- Path building.
- Query encoding.
- Header construction.
- Request body encoding.
- Connection management.
- Network execution.
- Response buffering.
- Response parsing.
- Native object storage.
- Retry decisions.
- Error normalization.
- Resource cleanup.

Kotlin and Swift should receive results, not raw protocol work.

## 5.2 Compile Time Over Runtime

Prefer:

- Generated encoders over reflection.
- Generated decoders over dynamic field mapping.
- Generated endpoint descriptors over runtime annotation scanning.
- Static operation IDs over runtime string lookup.
- Precomputed metadata over repeated runtime parsing.
- Link-time removal over runtime feature switches where possible.

Avoid runtime reflection and dynamic registries in the hot path.

## 5.3 Minimize Boundary Crossings

Platform bridges are expensive architectural boundaries.

Crossa must:

- Batch data crossing the boundary.
- Avoid one JNI call per model field.
- Avoid one native callback per tiny runtime event.
- Avoid building a complete duplicate object graph on both native and managed heaps.
- Prefer handles, native views, shared buffers, and coarse-grained operations.

## 5.4 Minimize Copies

The target pipeline should approximate:

```text
Network bytes
    ->
Reusable/native buffer
    ->
Native parser
    ->
Native typed storage/views
    ->
Platform-visible object/view
```

Crossa should not routinely perform:

```text
Native buffer
    ->
C++ string
    ->
JNI string
    ->
Kotlin string
    ->
another DTO
```

unless ownership or platform ergonomics require materialization.

## 5.5 Explicit Ownership

Every buffer, object, view, request, response, stream, database row view, and socket payload must have an explicit lifetime owner.

Crossa must never depend on accidental lifetime coupling.

## 5.6 Stable ABI at Public Boundaries

Crossa internals may use modern C++ aggressively.

The durable binary boundary must remain small and controlled.

Do not expose unstable implementation details such as STL containers as the primary public ABI.

## 5.7 Modules Depend on the Runtime, Not on Each Other

Networking must not become the foundation of Database.

Database must not know about WebSocket.

WebSocket must not know about HTTP models.

Shared functionality belongs in runtime/core abstractions.

## 5.8 Benchmark Before Claiming Performance

Crossa must never market or architect around unmeasured assumptions.

Every optimization must be evaluated across:

- Latency.
- CPU.
- Memory.
- Allocations.
- Copies.
- JNI/Swift boundary activity.
- Binary size.
- Battery/energy where measurable.
- Cold initialization.
- Sustained workloads.

---

# 6. High-Level Repository Architecture

The project should start as a monorepo to allow coordinated compiler/runtime/SDK evolution.

Recommended logical structure:

```text
crossa/
|
|-- docs/
|   |-- architecture/
|   |-- decisions/
|   |-- performance/
|   |-- compatibility/
|   |-- security/
|   `-- roadmap/
|
|-- compiler/
|   |-- frontend/
|   |-- semantic/
|   |-- ir/
|   |-- optimizer/
|   |-- linker/
|   |-- generators/
|   |-- diagnostics/
|   `-- tooling/
|
|-- contract/
|   |-- model/
|   |-- validation/
|   |-- compatibility/
|   `-- serialization/
|
|-- runtime/
|   |-- core/
|   |-- memory/
|   |-- scheduler/
|   |-- buffers/
|   |-- objects/
|   |-- errors/
|   |-- observability/
|   |-- security/
|   `-- platform/
|
|-- modules/
|   |-- network/
|   |-- database/
|   |-- websocket/
|   |-- socket/
|   |-- binary-socket/
|   |-- cache/
|   |-- files/
|   |-- compression/
|   |-- crypto/
|   `-- telemetry/
|
|-- adapters/
|   |-- nestjs/
|   |-- openapi/
|   |-- spring/
|   `-- ...
|
|-- bindings/
|   |-- android/
|   |-- ios/
|   `-- shared-abi/
|
|-- packaging/
|   |-- android/
|   |-- ios/
|   `-- manifests/
|
|-- benchmarks/
|   |-- android/
|   |-- ios/
|   |-- native/
|   `-- datasets/
|
|-- examples/
|   |-- backend/
|   |-- android/
|   `-- ios/
|
|-- tools/
|   |-- crossa-cli/
|   |-- schema-inspector/
|   |-- compatibility-checker/
|   `-- artifact-inspector/
|
`-- third_party/
```

Not every future module must be implemented immediately.

Directories for future modules may initially contain only architecture contracts or remain absent until development begins. The important rule is that the runtime boundaries are designed so these modules can be added without restructuring networking.

---

# 7. Layering Rules

Crossa must enforce a strict dependency direction.

```text
Backend Adapters
       |
       v
Contract Model
       |
       v
Compiler Frontend
       |
       v
Semantic Model
       |
       v
Crossa IR
       |
       v
Optimizer / Linker
       |
       v
Generators
       |
       v
Generated Native Module
       |
       v
Crossa Runtime
       |
       v
Stable ABI
       |
       v
Android / iOS Bindings
```

Runtime modules must follow:

```text
Platform Bindings
       |
       v
Stable ABI
       |
       v
Runtime Core
       ^
       |
  Runtime Modules
```

Forbidden dependencies include:

- Runtime core depending on Android.
- Runtime core depending on Swift.
- Compiler depending on JNI.
- Network module depending on NestJS.
- Database depending on network DTOs.
- Generated model layout depending directly on Kotlin data-class implementation details.
- Backend adapters generating platform source directly.
- Platform bindings owning core request parsing behavior.

---

# 8. Core Runtime Architecture

The runtime is the long-lived native engine used by all future Crossa modules.

It should provide:

## 8.1 Runtime Lifecycle

A Crossa runtime instance owns:

- Runtime configuration.
- Allocators.
- Buffer pools.
- Scheduler.
- Worker resources.
- Module registry.
- Logging bridge.
- Metrics bridge.
- Native object registry.
- Platform callbacks.
- Feature configuration.
- Shutdown coordination.

Runtime initialization must be explicit and measurable.

The runtime must not perform unexpected heavyweight work on a UI/main thread.

## 8.2 Memory System

The runtime must contain memory strategies for distinct lifetimes:

### Persistent Runtime Memory

Used for:

- Runtime configuration.
- Connection metadata.
- Module state.
- Caches.
- Long-lived lookup tables.

### Request Memory

Used during one request/operation.

It should be releasable as a unit.

### Response Arena

Used for response model data where arena allocation improves locality and cleanup.

### Stream/Socket Buffers

Long-lived or reusable buffers independent of ordinary request arenas.

### Scratch Memory

Short-lived memory used by encoding/parsing/transformations.

The project must prefer:

- Reusable buffers.
- Pooling where measurements justify it.
- Arena/monotonic allocation for grouped lifetime data.
- Bounded growth.
- Explicit maximum capacities.
- Predictable destruction.

Pooling must not be added blindly. Pools that retain large peak allocations indefinitely can become a memory regression on mobile devices.

## 8.3 Object Runtime

Generated models should use an internal object representation designed for:

- Stable field layout metadata.
- Cheap primitive access.
- Compact presence/nullability state.
- Efficient string references.
- Efficient lists.
- Nested objects.
- Native lifetime tracking.
- Optional lazy materialization.
- Platform view generation.

The object system must support future data sources beyond JSON.

A network response, database row batch, WebSocket message, and binary socket message should be capable of mapping into the same broad native object/value system.

That avoids creating one object infrastructure per module.

## 8.4 Scheduler

Crossa needs one coordinated scheduler architecture rather than independent thread pools per module.

It should distinguish:

- Network event loop work.
- CPU-heavy parsing.
- Database work.
- Compression.
- Cryptography.
- Stream processing.
- Platform callback dispatch.

The scheduler must:

- Avoid one-thread-per-request.
- Avoid unbounded queues.
- Support cancellation.
- Support deadlines/timeouts.
- Support priority metadata later.
- Support cooperative runtime shutdown.
- Expose metrics.
- Avoid excessive Android JNI thread attachment.

---

# 9. Stable Native ABI

Crossa requires a small, versioned ABI between the native runtime and platform bindings.

The ABI design must prioritize:

- Binary stability.
- Explicit handles.
- Explicit ownership.
- Explicit release operations.
- Error-code stability.
- Version negotiation.
- Optional capability discovery.
- Safe buffer access.
- Bulk operations.

The ABI should not expose Crossa's internal class hierarchy.

The ABI must not make the entire runtime ABI-dependent on:

- STL container layout.
- Compiler-specific class ABI.
- Exceptions crossing boundaries.
- RTTI assumptions.
- Internal allocator types.

Crossa can use C++ internally while maintaining a narrow C-compatible ABI where stability is valuable.

---

# 10. Android Architecture

## 10.1 Packaging

Android distribution should produce an AAR containing:

- Kotlin public API/bindings.
- JNI integration.
- Required native `.so` libraries by supported ABI.
- Generated module metadata.
- Consumer ProGuard/R8 rules only when genuinely needed.
- Version/capability manifest.

## 10.2 JNI Rules

JNI must be treated as an optimization boundary.

The binding layer must:

- Minimize marshalled data.
- Minimize call count.
- Register native methods explicitly where appropriate.
- Cache required JNI IDs safely during initialization.
- Avoid repeated class/method lookup.
- Avoid creating large managed object graphs for native results by default.
- Avoid attaching arbitrary native worker threads to the VM unless required.
- Keep JNI code concentrated in a small, auditable area.

## 10.3 Android Object Exposure

Crossa should support native-backed model views.

The default architecture should allow a response to remain stored in native memory while Kotlin receives a lightweight handle/view.

For large collections:

- Do not eagerly create thousands of JVM objects unless explicitly requested.
- Provide indexed/native-backed access.
- Materialize only when platform code requires ownership independent of the native response.
- Keep lifetime semantics clear.

## 10.4 Coroutines

Kotlin coroutines may be used as the ergonomic application API.

They must not become the owner of networking execution.

Coroutines should bridge to completion/cancellation primitives from Crossa rather than reimplement the request pipeline.

---

# 11. iOS Architecture

## 11.1 Packaging

iOS distribution should produce:

- XCFramework.
- Stable public native headers.
- Swift-facing wrapper/API.
- Optional Swift Package wrapper.
- Symbol/version metadata.
- Debug symbols as separate release artifacts where appropriate.

## 11.2 Interoperability Strategy

Swift supports direct C++ interoperability, but Crossa should avoid making public API stability depend on exposing the entire C++ implementation.

Preferred model:

```text
Swift API
    |
Stable Crossa ABI / carefully selected C++ interop
    |
Crossa Runtime
```

Direct Swift/C++ interoperability can be used where it creates a measurable benefit and has well-defined lifetime semantics.

## 11.3 Container Crossing

Do not blindly return large C++ STL collections into Swift.

Platform interoperability behavior can introduce copies depending on usage.

Crossa should instead provide:

- Native-backed collection views.
- Stable indexed access.
- Iteration designed around ownership guarantees.
- Bulk materialization only when explicitly requested.

---

# 12. Networking Module — V1 Product

Networking is the first module because it demonstrates the full compiler-to-runtime architecture.

The V1 network module must cover:

## 12.1 Contract Capabilities

- HTTP methods.
- URL/base URL configuration.
- Path parameters.
- Query parameters.
- Headers.
- Body payloads.
- JSON request/response.
- Nullable and optional values.
- Lists and nested models.
- Response status mapping.
- Typed errors.
- Authentication metadata.
- Timeouts.
- Cancellation.
- Request IDs/operation IDs.
- Multipart upload.
- File download foundations.
- Configurable retry policy.
- Basic redirect policy.
- Response metadata.
- Debug observability.

## 12.2 Generated Request Pipeline

The compiler must generate endpoint-specific execution metadata/code.

The runtime must not repeatedly parse a generic endpoint description for every call.

The generated pipeline should know in advance:

- Operation identifier.
- HTTP method.
- Static path segments.
- Parameter locations.
- Required fields.
- Static headers.
- Dynamic header sources.
- Request content type.
- Expected response types.
- Error response variants.
- Authentication behavior.
- Retry eligibility.
- Timeout defaults.

## 12.3 Request Encoding

Crossa should generate schema-specific encoders.

Avoid constructing a generic JSON DOM just to serialize a known typed request.

The encoder architecture must support:

- Direct writes to a reusable output buffer.
- Correct escaping.
- Primitive specialization.
- Nested object traversal.
- Nullable/optional behavior.
- Collection encoding.
- Predictable capacity growth.
- Size limits.

## 12.4 HTTP Transport

The initial candidate transport is libcurl.

The runtime should use an asynchronous/event-oriented approach suitable for many concurrent transfers, not one blocking thread per request.

The network engine should own:

- Reused transfer resources.
- Connection reuse.
- DNS/cache behavior.
- TLS configuration.
- HTTP/1.1 fallback.
- HTTP/2 support.
- Timeouts.
- Cancellation.
- Redirect policy.
- Backpressure integration for streaming.
- Metrics.

HTTP/3 should remain a later capability until the V1 architecture and benchmark suite are stable.

## 12.5 TLS

TLS is a first-class security dependency.

Crossa must:

- Pin exact dependency versions.
- Track security advisories.
- Support timely CVE upgrades.
- Verify certificate validation defaults.
- Never disable verification in production defaults.
- Document CA-store strategy per platform.
- Separate development overrides from release behavior.
- Treat binary-size cost as a measurable requirement.

For Android, libcurl cannot simply use Android's native SSL/TLS layer, so the chosen curl build will need an appropriate TLS backend. This dependency must be treated as part of Crossa's security lifecycle.

## 12.6 Response Buffering

Network bytes must be written into native buffers.

The buffer system should support:

- Reuse.
- Capacity limits.
- Size-aware growth.
- Large-response handling.
- Cancellation-safe release.
- Parser padding/alignment requirements.
- Streaming later without requiring full buffering.

## 12.7 Response Parsing

Initial JSON parsing candidate: simdjson.

Crossa should use generated schema-specific parsing logic rather than generic reflection.

The parser system must:

- Reuse parser resources when beneficial.
- Avoid generic DOM construction when not required.
- Validate required fields.
- Track nullability.
- Normalize parse errors.
- Avoid unsafe references beyond buffer lifetime.
- Support controlled unknown-field behavior.
- Support forward compatibility policies.
- Record parser metrics when debug/performance mode is enabled.

## 12.8 Native Response Models

After parsing, platform clients should receive a usable object/view, not JSON.

For large payloads Crossa should prefer:

- Native-backed results.
- Compact layout.
- Arena ownership.
- Lazy platform object creation.
- Native-backed collections.
- Minimal string copying where lifetime permits.

---

# 13. Error Architecture

Crossa needs one error system shared by all modules.

Top-level error domains should eventually include:

- Contract.
- Compiler.
- Runtime.
- Network.
- Protocol.
- Authentication.
- Serialization.
- Deserialization.
- Timeout.
- Cancellation.
- Database.
- Socket.
- WebSocket.
- Binary protocol.
- Security.
- Internal invariant.

Errors crossing the public ABI must:

- Have stable numeric/category identifiers.
- Carry optional structured metadata.
- Avoid leaking internal exception objects.
- Be convertible into ergonomic Kotlin and Swift errors.
- Preserve retryability and recoverability metadata where applicable.

Exceptions must not cross native/platform boundaries.

---

# 14. Cancellation, Timeouts, and Lifecycle

Every long-running Crossa operation must have an operation handle/state.

The design must support:

- Cancel before execution.
- Cancel while queued.
- Cancel during transport.
- Cancel during parsing where practical.
- Runtime shutdown.
- App lifecycle transitions.
- Request timeout.
- Connection timeout.
- Read/write timeout where meaningful.
- Deadline propagation in future modules.

Cancellation must be race-safe and idempotent.

---

# 15. Observability

Performance software without observability becomes unmaintainable.

Crossa must provide optional instrumentation for:

- Operation start/end.
- DNS time.
- Connect time.
- TLS time.
- Time to first byte.
- Download duration.
- Bytes sent/received.
- Encode time.
- Parse time.
- Native object construction time.
- Boundary delivery time.
- Allocations/arena size where feasible.
- Retry count.
- Connection reuse.
- Protocol negotiated.
- Error category.

Observability must be:

- Cheap when disabled.
- Structured.
- Privacy-aware.
- Pluggable into Android/iOS logging/telemetry.
- Independent from a specific analytics provider.

---

# 16. Compiler Architecture

The compiler is as important as the runtime.

## 16.1 Pipeline

```text
Input Contract
    |
    v
Normalization
    |
    v
Validation
    |
    v
Semantic Model
    |
    v
Crossa IR
    |
    v
Optimization
    |
    v
Module Linking
    |
    v
Native Generation
    |
    +--> C++ generated module code
    |
    +--> Android binding generation
    |
    +--> iOS binding generation
    |
    +--> Metadata/manifests
```

## 16.2 Compiler Requirements

The compiler must be:

- Deterministic.
- Incremental where possible.
- Cacheable.
- Reproducible.
- Version-aware.
- Capable of clear diagnostics.
- Able to distinguish warnings and fatal compatibility errors.
- Independent from Android/iOS toolchains until platform generation/build stages.
- Capable of emitting a machine-readable build manifest.

## 16.3 Crossa IR

Crossa IR is the foundation for future expansion.

The IR must be able to represent:

- Type definitions.
- Operations.
- Models.
- Fields.
- Constraints.
- Transport operations.
- Serialization plans.
- Response variants.
- Errors.
- Ownership hints.
- Module references.
- Future database operations.
- Future socket messages.
- Future streaming contracts.

Do not hard-code IR around REST-only assumptions.

## 16.4 Optimization Passes

Future optimization passes can include:

- Dead operation elimination.
- Dead model elimination.
- Shared constant pooling.
- Static string interning.
- Field layout optimization.
- Serializer specialization.
- Decoder specialization.
- Duplicate model canonicalization.
- Module pruning.
- Feature/capability pruning.
- Compact operation tables.
- Link-time removal of unused runtime capabilities.

Optimization must preserve observable contract behavior.

---

# 17. Crossa Linker

Crossa should contain a linker-like stage.

Its purpose is to produce a client artifact containing only what is needed.

Inputs:

- Compiled Crossa modules.
- Required API groups.
- Runtime capabilities.
- Platform target.
- Feature configuration.

Outputs:

- Linked native module.
- Required runtime components.
- Generated binding surface.
- Artifact manifest.

This enables future use cases where a very large backend contract exists but one application only consumes selected services.

---

# 18. Backend Adapter Architecture

Backend adapters must be plugins around the contract layer.

The first adapter can target NestJS, but the compiler must never assume NestJS.

Adapter responsibilities:

- Discover operations.
- Discover DTO/model shapes.
- Resolve required/optional fields.
- Resolve route metadata.
- Resolve validation metadata.
- Resolve authentication metadata when explicitly supported.
- Normalize metadata.
- Produce Crossa contract output.
- Emit diagnostics for unsupported constructs.

Adapter non-responsibilities:

- Generate Android code.
- Generate Swift code.
- Compile C++.
- Implement networking.
- Know JNI.
- Know XCFramework packaging.

This separation allows future adapters without changing the runtime.

---

# 19. Versioning and Compatibility

Crossa has multiple independent version axes:

1. CLI/compiler version.
2. Contract schema version.
3. IR version.
4. Runtime ABI version.
5. Runtime implementation version.
6. Module API version.
7. Generated SDK version.
8. Backend contract version.

These must not be collapsed into one integer.

## 19.1 Compatibility Checker

Crossa must eventually detect backend contract changes such as:

- Removed operation.
- Renamed operation.
- Changed parameter type.
- Changed requiredness.
- Removed required response field.
- Changed response type.
- Changed enum compatibility.
- Changed authentication requirement.
- Changed status/error contract.

CI should be able to fail on disallowed breaking changes.

## 19.2 Artifact Manifest

Every generated SDK should contain metadata describing:

- Compiler version.
- Runtime ABI.
- Runtime version.
- Contract hash.
- Build timestamp or reproducible build metadata.
- Enabled modules.
- Target architectures.
- Dependency versions.
- Backend adapter version.
- Schema version.
- Build configuration.

---

# 20. Build System

## 20.1 Native Build

Use CMake as the primary native build description.

Use Ninja for fast native builds where applicable.

Requirements:

- Pinned toolchain versions.
- Reproducible release configuration.
- Separate debug/release profiles.
- Symbol visibility control.
- Link-time optimization evaluation.
- Dead stripping.
- Hidden visibility by default.
- Minimal exported symbols.
- Architecture-specific builds.
- Build cache support.
- Deterministic generated source paths.

## 20.2 Android Build

Android should use:

- Android NDK.
- CMake integration.
- Gradle packaging.
- AAR generation.
- ABI selection.
- Native symbol packaging policy.

Initially support:

- `arm64-v8a` as the primary production target.
- Additional required Android ABIs only when product requirements justify them.

Do not increase distribution size without measured justification.

## 20.3 iOS Build

iOS should build device/simulator slices required by distribution policy and package them into an XCFramework.

Release packaging must:

- Strip unnecessary symbols.
- Keep dSYM/symbol artifacts separate as appropriate.
- Avoid shipping headers/internal APIs that consumers do not need.
- Preserve only the intended public surface.

---

# 21. Dependency Policy

Crossa will rely on carefully selected native dependencies.

Every dependency must pass evaluation for:

- License.
- Mobile platform support.
- Maintenance activity.
- Security history.
- ABI behavior.
- Binary size.
- CPU/memory behavior.
- Build complexity.
- Upgrade difficulty.
- Platform architecture support.

Initial networking candidates:

- libcurl.
- HTTP/2 dependency required by selected curl build.
- TLS backend selected and maintained by Crossa.
- simdjson for JSON parsing.

Dependencies must be wrapped behind Crossa-owned abstractions.

No third-party type should become foundational public ABI unless unavoidable.

This allows replacement later.

---

# 22. Security Rules

Crossa will eventually sit beneath authentication and network security for applications, so security cannot be an afterthought.

Required rules:

- Secure defaults.
- Certificate verification enabled.
- No silent downgrade to insecure protocols.
- No secret values in ordinary logs.
- Sensitive buffers cleared when necessary and justified.
- Bounds checked at external input boundaries.
- Response/body size limits.
- Header size limits.
- Parser depth limits.
- Database limits later.
- Socket frame/message limits later.
- Fuzzing-ready parser/decoder architecture.
- Dependency CVE monitoring.
- Reproducible dependency inventory.
- Release SBOM generation in a later milestone.
- Sanitizer builds in CI for native development.
- Hardened production compiler/linker settings.

All input from network, socket, database file, generated artifact, and external contract source must be treated as untrusted until validated.

---

# 23. Future Module Architecture

The runtime must be designed now so future modules do not force a rewrite.

## 23.1 Database Module

Future objective:

> Generated, native, typed persistence powered by C++ with platform applications receiving ready objects/views.

Potential responsibilities:

- SQLite-backed storage initially.
- Prepared statements.
- Generated row decoders.
- Typed query plans.
- Transactions.
- Migrations.
- Batch reads.
- Native row/object views.
- Change observation.
- Cache integration.
- Encryption extension points.

Database must share:

- Runtime scheduler.
- Native object model.
- Error model.
- Memory subsystem.
- Observability.
- ABI conventions.

It must not reuse networking abstractions improperly.

## 23.2 WebSocket Module

Responsibilities:

- Connection lifecycle.
- Upgrade/handshake.
- Reconnect policy.
- Heartbeat/ping/pong.
- Message framing.
- Backpressure.
- Typed generated message decoders.
- Native message buffers.
- Authentication integration.
- Connection observability.

WebSocket should be integrated as a module even if libcurl provides part of the underlying WebSocket capability.

## 23.3 Raw Socket Module

Purpose:

- TCP/UDP-oriented future workloads.
- Custom protocols.
- Direct streaming protocols.

Responsibilities:

- Connection lifecycle.
- Send/receive buffers.
- Backpressure.
- Timeouts.
- Cancellation.
- Framing extension points.
- Security/TLS extension points.

The implementation can select appropriate native/platform networking primitives while keeping Crossa's C++ runtime in control of execution and object processing.

## 23.4 Binary Socket Module

This should be built for schema-defined binary protocols.

Capabilities may include:

- Fixed/variable frame headers.
- Length-prefixed messages.
- Binary schemas.
- Compact typed encoders/decoders.
- Checksums.
- Compression.
- Streaming decode.
- Endianness handling.
- Zero-copy payload views where safe.

The compiler and IR should eventually support protocol/message definitions without requiring changes to the core type system.

## 23.5 Cache Module

Could provide:

- Memory cache.
- LRU/size-bound strategies.
- Serialized/native-object cache.
- Database-backed cache later.
- Expiry.
- Invalidation.
- Request coalescing integration.

## 23.6 Crypto Module

Crossa should not invent cryptographic algorithms.

A crypto module would provide:

- Stable Crossa abstractions.
- Vetted backend implementations.
- Hashing.
- MAC.
- Encryption/decryption where required.
- Key-provider integration.
- Secure random.
- Signing/verification.

## 23.7 Compression Module

Potential algorithms:

- gzip/deflate where protocol-required.
- Brotli where useful.
- Zstandard for Crossa-owned binary/storage formats if justified.

Compression belongs behind runtime interfaces and must be benchmarked for mobile CPU/battery tradeoffs.

---

# 24. Platform Capability Escape Hatches

A strict C++ core does not mean Crossa should ignore platform constraints.

Crossa must support narrowly defined platform adapters where operating-system behavior cannot be reproduced correctly from a portable runtime.

Examples may include:

- iOS background transfer semantics.
- OS credential stores.
- OS network reachability/path signals.
- Android/iOS lifecycle signals.
- Secure key storage.
- Platform-specific proxy/certificate policies.

The rule is:

> Platform services may provide capabilities to the C++ runtime, but platform code must not duplicate Crossa's contract, parsing, serialization, object, or business execution pipeline.

This preserves the architecture while respecting mobile OS behavior.

---

# 25. iOS Distribution and Runtime Restrictions

Crossa must be designed primarily around **build-time generated/compiled code** for App Store applications.

The standard production model is:

```text
Backend contract
    ->
Crossa compiler in development/CI
    ->
Generated native SDK
    ->
Included in application bundle
```

Do not make the initial architecture depend on downloading new executable Crossa logic after App Store release.

Remote configuration/data can be supported later where it remains data and does not violate platform rules regarding downloaded executable functionality.

---

# 26. Performance Model

Crossa should define performance around the complete client pipeline.

For a network operation, benchmark:

```text
T0 request initiated by application
T1 request encoded
T2 transport started
T3 first response byte
T4 response fully received
T5 response parsed
T6 result accessible by platform application
```

Server/network latency can dominate wall-clock time, so Crossa must separately measure client overhead.

Core metrics:

- Request encoding CPU time.
- Parse CPU time.
- Result delivery overhead.
- Native allocations.
- Managed allocations.
- Peak native memory.
- Peak managed memory.
- Number of copies.
- JNI crossings.
- Swift/native boundary crossings where measurable.
- GC pressure on Android.
- Binary size.
- Runtime initialization time.
- Connection reuse.
- CPU under concurrency.
- Energy impact where measurable.

---

# 27. Performance Baselines

Crossa must continuously compare itself against realistic alternatives.

Android baselines can include:

- OkHttp + common generated/manual API layer.
- Retrofit + OkHttp.
- Ktor Client.
- kotlinx.serialization where relevant.

iOS baselines can include:

- URLSession + Codable.
- Other representative generated-client approaches where useful.

The goal is not to win every microbenchmark.

Crossa must demonstrate strong results in the workloads it is designed for:

- Large responses.
- Large lists.
- Repeated requests.
- High request concurrency.
- Reused connections.
- Serialization-heavy payloads.
- Parsing-heavy payloads.
- Low-memory mobile scenarios.
- High-frequency real-time messages later.

---

# 28. Performance Gates

Before declaring a subsystem optimized:

- Measure it on physical Android ARM64 devices.
- Measure it on physical iOS ARM64 devices.
- Measure cold and warm behavior.
- Measure small and large payloads.
- Measure memory, not only time.
- Measure realistic release builds.
- Avoid relying on desktop-only benchmarks.
- Record toolchain and device metadata with benchmark results.

Performance regressions should be treated as release signals, not informal observations.

Thresholds can be defined after the first stable baseline rather than invented now.

---

# 29. Binary Size Strategy

Native performance can become unacceptable if every generated SDK ships a massive runtime.

Crossa must therefore support:

- Static capability pruning.
- Hidden visibility.
- Dead stripping.
- LTO evaluation.
- Module-level linking.
- Removal of unused protocols.
- Removal of unused generated APIs.
- Architecture-targeted distribution.
- Optional feature packaging.
- Shared constant tables.
- Compact model metadata.
- Minimal platform wrappers.

Each feature should have a measurable binary-size cost.

---

# 30. Scalability Definition

“Scalable” for Crossa means more than handling many HTTP requests.

The project must scale across:

## Codebase Scale

- Hundreds of runtime components.
- Many compiler passes.
- Multiple platform bindings.
- Multiple backend adapters.

## API Scale

- Thousands of backend operations.
- Thousands of models.
- Large shared contracts.
- Multiple backend services.

## Runtime Scale

- Many concurrent requests.
- Large payloads.
- Real-time streams.
- Large database result sets.
- Many socket messages.

## Team Scale

- Multiple engineers working independently.
- Stable module ownership.
- Enforced dependency rules.
- Clear public/internal APIs.
- Architecture decision records.

## Product Scale

- Networking first.
- Storage later.
- Realtime later.
- New protocols later.
- New platform targets later.

---

# 31. Modularity Rules

Every runtime module must define:

1. Public capability contract.
2. Internal implementation.
3. Dependencies on runtime services.
4. Memory behavior.
5. Scheduler behavior.
6. Error domain.
7. Observability events.
8. Compiler/IR requirements if any.
9. Platform integration requirements.
10. Binary-size impact.
11. Version/capability identifier.

A module must never mutate unrelated runtime global state.

Cross-module communication should happen through explicit runtime interfaces or shared core types.

---

# 32. Concurrency and Thread-Safety Rules

Crossa must assume concurrent use from the beginning.

Rules:

- No unsynchronized process-global mutable application state.
- Immutable generated metadata where practical.
- Explicit ownership for mutable runtime state.
- Clear locking policy.
- Avoid lock contention on hot paths.
- Use atomics only when semantics are clear.
- Avoid blocking the network event loop.
- Avoid parsing huge payloads on a UI/main thread.
- Avoid unbounded worker creation.
- Use bounded queues.
- Cancellation must be race-safe.
- Destruction must coordinate in-flight operations.
- Logging/metrics must not introduce hot-path contention.

---

# 33. C++ Engineering Standards

Use modern C++ with a conservative production mindset.

Primary goals:

- RAII.
- Explicit ownership.
- Const-correctness.
- Move semantics where useful.
- `std::span`/views where lifetime-safe.
- `std::string_view` only when ownership is guaranteed.
- `std::pmr` where measurements justify arena/resource allocation.
- Strong domain types where they prevent errors.
- No raw owning pointers.
- Minimal shared ownership.
- No exception crossing ABI boundaries.
- No macros for ordinary architecture.
- No hidden global initialization with expensive work.
- No undefined-behavior-dependent optimization.
- No premature template complexity that harms compile times without measurable runtime value.

Performance code should remain readable and auditable.

---

# 34. Public API Design Rules

Even though the runtime is C++, Crossa must feel native to platform developers.

Android API should be:

- Kotlin-first.
- Nullability-correct.
- Coroutine-friendly.
- Discoverable.
- Small.
- Generated where appropriate.
- Free of JNI details.
- Free of raw pointers.

iOS API should be:

- Swift-friendly.
- `async/await` friendly.
- Nullability-correct.
- Safe in ownership semantics.
- Free of C++ implementation details unless intentionally exposed.
- Free of raw lifetime management for normal consumers.

Generated SDK users should not need to understand Crossa internals.

---

# 35. Developer Experience

The CLI should eventually support workflows such as:

```text
extract
validate
build
inspect
diff
benchmark
doctor
clean
version
```

Exact CLI syntax is not specified by this document.

Developer experience objectives:

- One deterministic build command.
- Clear diagnostics.
- Generated artifact summary.
- Contract diff report.
- Dependency version report.
- Binary size summary.
- Enabled module summary.
- Actionable failures.
- Easy CI integration.

---

# 36. Generated Artifact Outputs

A complete Crossa SDK build should eventually produce conceptually:

```text
dist/
|
|-- android/
|   |-- crossa-<sdk>.aar
|   `-- metadata/
|
|-- ios/
|   |-- Crossa<SDK>.xcframework
|   |-- Package.swift
|   `-- metadata/
|
|-- contract/
|   |-- normalized-contract
|   `-- contract-hash
|
|-- reports/
|   |-- compatibility
|   |-- size
|   |-- dependencies
|   `-- build
|
`-- manifests/
    `-- artifact-manifest
```

Exact names can evolve.

---

# 37. CI/CD Architecture

The CI pipeline should eventually contain these logical gates:

```text
Contract Extraction
        |
        v
Contract Validation
        |
        v
Compatibility Analysis
        |
        v
Compiler Build
        |
        v
Generated Native Sources
        |
        v
Native Build
    /         \
Android      iOS
   |           |
   v           v
Artifact Inspection
        |
        v
Native Safety Validation
        |
        v
Performance Regression Checks
        |
        v
Packaging
        |
        v
Release Artifacts
```

Release artifacts must be reproducible enough to investigate differences.

CI must pin:

- C++ compiler/toolchain.
- NDK.
- CMake.
- Native dependencies.
- Xcode/Swift toolchain.
- Crossa compiler version.

---

# 38. Documentation Requirements

Crossa must maintain documentation as engineering infrastructure.

Required document families:

## Architecture

- Runtime overview.
- Compiler overview.
- IR overview.
- ABI specification.
- Module architecture.
- Memory architecture.
- Scheduler architecture.

## Platform

- Android binding model.
- iOS binding model.
- Packaging.
- Lifecycle integration.

## Performance

- Benchmark methodology.
- Device matrix.
- Baselines.
- Optimization decisions.
- Regression history.

## Security

- Threat model.
- Dependency security.
- TLS decisions.
- Parser/input limits.
- Release hardening.

## Decisions

Use ADRs for major decisions such as:

- Transport engine.
- TLS backend.
- JSON parser.
- ABI model.
- Native object layout.
- Swift integration strategy.
- JNI exposure strategy.
- Database backend later.

Do not bury architectural decisions only inside pull requests.

---

# 39. Roadmap

The roadmap deliberately builds architecture before syntax.

---

## Phase 0 — Project Foundation

### Goal

Create the project structure and engineering rules.

### Build

- Monorepo.
- CMake root.
- Runtime core skeleton.
- Compiler skeleton.
- Contract model package.
- Shared ABI package.
- Android binding project.
- iOS binding project.
- CI skeleton.
- Documentation structure.
- ADR process.
- Dependency policy.

### Deliverable

A native runtime that can be built for:

- macOS development host.
- Android ARM64.
- iOS ARM64.

No real Crossa language syntax is required.

---

## Phase 1 — Contract Model and IR

### Goal

Create the platform-independent representation Crossa will compile.

### Build

- Core type model.
- Model/field metadata.
- Operation metadata.
- Request/response metadata.
- Error metadata.
- Nullability.
- Collections.
- Enum representation.
- Contract validation.
- Deterministic contract serialization.
- Crossa IR V1.
- IR versioning.

### Deliverable

A contract can be loaded, validated, converted to IR, and inspected without any Android/iOS dependency.

---

## Phase 2 — Native Runtime Core

### Goal

Build infrastructure every future Crossa module will share.

### Build

- Runtime lifecycle.
- Error model.
- Handle registry.
- Memory resources.
- Buffer primitives.
- Response arena.
- Scheduler foundations.
- Cancellation primitives.
- Logging hooks.
- Metrics hooks.
- Capability/version registry.
- Stable ABI V1.

### Deliverable

Android and iOS can initialize/shutdown the same native runtime and exchange controlled handles/results.

---

## Phase 3 — Android and iOS Native Object Bridge

### Goal

Solve the most important platform-boundary architecture before networking becomes large.

### Build

Android:

- JNI registration.
- Handle lifetime.
- Native-backed primitives/models.
- Native-backed collection view.
- Safe release.
- Coroutine completion bridge.

iOS:

- Native handle/view model.
- Swift wrapper.
- Safe lifetime rules.
- Native-backed collection strategy.
- Async completion bridge.

### Deliverable

A native object graph can be exposed efficiently to Android and iOS without eager duplication of the full object graph.

---

## Phase 4 — Networking Transport

### Goal

Build the persistent C++ transport engine.

### Build

- libcurl integration.
- Async/multi model.
- Connection reuse.
- HTTP/1.1.
- HTTP/2.
- TLS backend.
- DNS behavior.
- Timeout support.
- Cancellation.
- Header support.
- Request body support.
- Response buffering.
- Transport metrics.
- Resource cleanup.

### Deliverable

Crossa C++ can execute production-grade HTTPS requests on Android and iOS and retain response bytes natively.

---

## Phase 5 — Generated Request Encoding

### Goal

Remove generic runtime request construction.

### Build

- Operation-specific request plans.
- Path encoder generation.
- Query encoder generation.
- Header plan generation.
- Generated JSON request encoders.
- Multipart foundation.
- Authentication-provider integration.
- Request buffer reuse.

### Deliverable

A compiled operation can create and send its request without platform-side request assembly or generic reflection.

---

## Phase 6 — Generated Response Parsing

### Goal

Convert response bytes directly into native typed objects.

### Build

- simdjson integration.
- Parser resource reuse.
- Generated decoders.
- Required/optional validation.
- Nested models.
- Collections.
- Error-body parsing.
- Native strings/views.
- Response arenas.
- Parse metrics.
- Size/depth limits.

### Deliverable

HTTPS response bytes become native typed Crossa objects without Kotlin Serialization or Swift Codable.

---

## Phase 7 — End-to-End Generated SDK

### Goal

Connect backend contract to production mobile artifacts.

### Build

- Compiler generators.
- Android Kotlin API generation.
- iOS Swift API generation.
- AAR packaging.
- XCFramework packaging.
- Artifact manifest.
- Version metadata.
- Contract hash.
- SDK initialization API.
- Typed errors.

### Deliverable

Backend contract -> Crossa build -> Android AAR + iOS XCFramework -> application receives ready typed objects.

This is the first complete Crossa product milestone.

---

## Phase 8 — NestJS Adapter

### Goal

Automate contract extraction from a real backend framework.

### Build

- NestJS project integration.
- Controller discovery.
- Route discovery.
- DTO discovery.
- Validation metadata.
- Auth metadata where safely inferable/configurable.
- Unsupported construct diagnostics.
- Normalized contract output.

### Deliverable

A NestJS backend can generate the contract that feeds the complete Crossa SDK pipeline.

---

## Phase 9 — Compatibility and Incremental Build System

### Goal

Make Crossa practical for large production teams.

### Build

- Contract hashing.
- Operation hashing.
- Model hashing.
- Incremental code generation.
- Build cache.
- Compatibility checker.
- Breaking-change classification.
- Crossa lock/build metadata.
- Dead operation elimination.
- Dead model elimination.
- Module pruning.

### Deliverable

Large API contracts rebuild efficiently and CI can detect incompatible backend/mobile changes.

---

## Phase 10 — Performance Hardening

### Goal

Prove Crossa's value using measured end-to-end performance.

### Build

- Android physical-device benchmark suite.
- iOS physical-device benchmark suite.
- Allocation tracking.
- Native-memory tracking.
- Large-payload benchmarks.
- High-concurrency benchmarks.
- Large-list benchmarks.
- Boundary-crossing metrics.
- Binary-size reporting.
- Connection reuse metrics.
- Parser tuning.
- Buffer tuning.
- LTO/dead-strip evaluation.

### Deliverable

A published internal performance report showing where Crossa wins, where it is equivalent, and where further optimization is required.

No unsupported performance claim is allowed before this phase.

---

## Phase 11 — Production Networking Features

### Goal

Make the networking module usable for broad production applications.

### Build

- Multipart.
- Upload progress.
- Download streaming.
- Retry policies.
- Authentication refresh coordination.
- Request deduplication/coalescing where useful.
- Cache integration hooks.
- Compression policies.
- Proxy configuration.
- Certificate policies.
- HTTP/3 evaluation.
- Structured telemetry.

### Deliverable

Crossa networking can replace repetitive application networking infrastructure for a serious production mobile project.

---

## Phase 12 — WebSocket Runtime Module

### Goal

Add real-time typed messaging without creating a second runtime.

### Build

- WebSocket connection lifecycle.
- Generated message contracts.
- Native message decode.
- Reconnect.
- Heartbeat.
- Backpressure.
- Cancellation.
- Authentication.
- Observability.
- Android/iOS stream bindings.

### Deliverable

Backend-defined WebSocket messages are consumed as ready native-backed objects on Android and iOS.

---

## Phase 13 — Database Runtime Module

### Goal

Extend the compiler/runtime model from remote data to local native data.

### Build

- SQLite integration evaluation.
- Typed schema model.
- Generated statements.
- Generated row decoders.
- Transactions.
- Migrations.
- Batch reads.
- Native-backed result collections.
- Scheduler integration.
- Change observation architecture.

### Deliverable

Crossa-generated database operations return the same family of typed native-backed objects used elsewhere in the runtime.

---

## Phase 14 — Raw and Binary Socket Modules

### Goal

Support high-throughput custom protocols.

### Build

- TCP/UDP abstractions.
- Connection lifecycle.
- Framing.
- Binary schemas.
- Generated binary encoders/decoders.
- Buffer slicing.
- Streaming decode.
- Backpressure.
- Compression hooks.
- Encryption hooks.
- Metrics.

### Deliverable

Crossa can power custom binary real-time protocols without requiring Kotlin/Swift to parse protocol payloads.

---

## Phase 15 — Crossa Language Syntax

### Goal

Design the developer-facing Crossa source language only after runtime and IR requirements are proven.

### Preconditions

Do not finalize language syntax before:

- IR V1 is proven.
- Network contract is proven.
- Object model is proven.
- Errors are proven.
- Module extension model is proven.
- Database/socket requirements have informed the type system.
- Backend adapters have revealed real contract requirements.

### Deliverable

A language design based on actual runtime/compiler requirements rather than aesthetics.

This document intentionally contains no Crossa syntax proposal.

---

# 40. Definition of V1 Success

Crossa V1 is successful when all of the following are true:

1. A supported backend contract can be extracted.
2. Crossa validates and compiles that contract.
3. Request/response handling is generated.
4. Network transport is C++ owned.
5. Request serialization is C++ owned.
6. Response parsing is C++ owned.
7. Typed native results are C++ owned.
8. Android receives a clean Kotlin API through an AAR.
9. iOS receives a clean Swift API through an XCFramework.
10. Application code does not manually parse JSON.
11. Application code does not duplicate endpoint DTO definitions.
12. Large responses do not require eager duplication into thousands of platform objects.
13. The runtime reuses connections and critical resources.
14. Performance is measured on physical devices.
15. Dependency versions and runtime ABI are reproducible.
16. The compiler can detect meaningful contract incompatibility.
17. The architecture can add WebSocket and Database modules without rewriting the runtime core.

---

# 41. What We Get When the Architecture Is Complete

A backend team changes or adds APIs.

Crossa then provides a deterministic pipeline:

```text
Backend
   |
   v
Contract Extraction
   |
   v
Crossa Contract
   |
   v
Compiler + IR
   |
   v
Generated Native Operations
   |
   v
Optimized C++ Runtime
   |
   +--------------------+
   |                    |
   v                    v
Android SDK           iOS SDK
   |                    |
   v                    v
Ready Objects         Ready Objects
```

Mobile teams get:

- Less duplicated networking code.
- One contract.
- One parser behavior.
- One serialization behavior.
- One error contract.
- One performance engine.
- One protocol implementation strategy.
- Stronger backend/mobile compatibility.
- Smaller managed-heap pressure for large payloads.
- A foundation that can later power persistence and real-time protocols.

Crossa maintainers get:

- One C++ core.
- Compiler-driven specialization.
- Platform wrappers instead of separate implementations.
- Measurable performance.
- A module architecture capable of growing over time.

---

# 42. Architectural Decisions That Are Fixed for the Initial Project

The following decisions should be treated as current project direction unless an ADR with benchmark/evidence replaces them:

- Crossa is C++-first.
- Android and iOS are first-class targets.
- Networking is the first production module.
- Request encoding happens in C++.
- Transport execution happens in the native runtime.
- Response buffering happens in native memory.
- Response parsing happens in C++.
- Native models are the source of truth.
- Platform bindings are thin.
- Crossa avoids eager full object duplication by default.
- The compiler owns specialization.
- Crossa IR precedes language syntax.
- Backend adapters target a neutral contract.
- Runtime modules share one core runtime.
- CMake is the native build system.
- The first transport candidate is libcurl.
- The first JSON parser candidate is simdjson.
- Public ABI remains intentionally small and versioned.
- Crossa language syntax is deferred.

---

# 43. Decisions That Must Remain Open Until Benchmarked

Do not prematurely lock:

- Exact TLS backend.
- Exact HTTP/3 stack.
- Exact native object binary layout.
- Exact string ownership representation.
- Exact list representation.
- Whether some responses should be lazily parsed versus eagerly parsed.
- Exact thread/worker counts.
- Exact buffer pool sizes.
- Exact arena sizes.
- Exact parser pool design.
- Exact Swift direct-C++ vs C-ABI split.
- Exact JNI fast-native usage.
- Exact LTO configuration.
- Exact compression algorithms.
- Exact database engine configuration.
- Exact binary protocol format.
- Crossa source syntax.

These decisions must be driven by production requirements and device benchmarks.

---

# 44. Rules for Coding Agents Working on Crossa

Any coding agent modifying Crossa must follow these instructions:

1. Read the architecture documents relevant to the subsystem before modifying code.
2. Preserve dependency direction.
3. Do not move work from C++ into Kotlin/Swift for convenience if it belongs to the hot path.
4. Do not expose new native ABI without documenting version/lifetime implications.
5. Do not expose STL implementation types as stable ABI by default.
6. Do not add global mutable state when state can belong to a runtime/module instance.
7. Do not add reflection to generated execution paths.
8. Do not add platform object materialization without considering native-backed views.
9. Do not create new thread pools inside individual modules without scheduler review.
10. Do not add a third-party dependency without dependency-policy review.
11. Do not make performance claims without measurements.
12. Do not add unbounded queues, buffers, caches, or pools.
13. Do not swallow native errors.
14. Do not let exceptions cross ABI boundaries.
15. Do not weaken TLS/certificate validation for convenience.
16. Do not couple backend adapters directly to platform bindings.
17. Do not alter IR incompatibly without an IR version/migration decision.
18. Do not design Crossa language syntax as part of unrelated tasks.
19. Keep generated code deterministic.
20. Prefer explicit ownership and RAII.
21. Keep hot-path logging optional and cheap.
22. Consider Android low-memory behavior.
23. Consider iOS lifecycle/background constraints.
24. Consider binary size for every native dependency and module.
25. Add or update ADRs for material architectural choices.

---

# 45. Engineering Review Checklist

Every substantial architecture change should be reviewed against:

### Correctness

- Is ownership explicit?
- Is cancellation safe?
- Are error paths deterministic?
- Are required inputs validated?
- Is behavior consistent on Android/iOS?

### Performance

- Does this add copies?
- Does this add allocations?
- Does this add JNI/Swift crossings?
- Does this block an event loop?
- Does this allocate per field/item?
- Does this retain large peak memory?
- Does this enlarge binaries materially?

### Architecture

- Does the dependency direction remain valid?
- Is this functionality in the correct module?
- Is the ABI affected?
- Is IR affected?
- Does this constrain future Database/Socket modules?

### Security

- Is external input bounded?
- Can malformed input cause excessive allocation?
- Are credentials/logging safe?
- Is certificate verification preserved?
- Is a new native dependency introduced?

### Platform

- Is Android lifecycle respected?
- Is JNI usage minimized?
- Is iOS lifecycle/background behavior respected?
- Are platform API restrictions considered?

---

# 46. Initial Technical Research Baseline

The initial design is informed by current official/project documentation:

- Android NDK supports building native C/C++ libraries with CMake, and Android's JNI guidance specifically recommends minimizing marshaling and the frequency/volume of JNI crossings.
- Swift supports C++ interoperability, but the interoperability feature continues to evolve and has documented constraints. Crossa therefore keeps a controlled public boundary rather than exposing its full internal C++ implementation.
- libcurl's multi interface supports asynchronous/event-driven handling of many simultaneous transfers, making it a strong initial candidate for Crossa's native transport engine.
- libcurl can be built for Android using the NDK; Android TLS requires a selected TLS backend rather than direct access to Android's native TLS implementation.
- simdjson provides ARM/NEON-aware parsing and explicitly recommends parser/buffer reuse for high-performance repeated parsing.
- libcurl includes WebSocket APIs, which can inform a future WebSocket module without forcing WebSocket behavior into the initial REST/HTTP product.
- Apple App Store rules make build-time compiled/generated SDKs the safest initial architecture; Crossa should not rely on downloading new executable functionality into released applications.
- Apple recommends URLSession for ordinary HTTP and provides Network framework for lower-level protocols; Crossa's portable native transport is therefore a deliberate product architecture choice and should be benchmarked and validated against platform behavior rather than assumed superior.

---

# 47. Reference Sources

Official/current sources used when establishing this blueprint:

- Android JNI performance guidance:  
  https://developer.android.com/ndk/guides/jni-tips

- Android NDK/CMake guidance:  
  https://developer.android.com/ndk/guides  
  https://developer.android.com/studio/projects/configure-cmake

- Swift and C++ interoperability:  
  https://www.swift.org/documentation/cxx-interop/  
  https://www.swift.org/documentation/cxx-interop/status/  
  https://www.swift.org/documentation/cxx-interop/safe-interop/

- libcurl API and multi interface:  
  https://curl.se/libcurl/c/  
  https://curl.se/libcurl/c/libcurl-multi.html

- libcurl Android build guidance:  
  https://curl.se/docs/install.html

- libcurl WebSocket documentation:  
  https://curl.se/docs/websocket.html

- simdjson performance guidance:  
  https://github.com/simdjson/simdjson/blob/master/doc/performance.md

- simdjson On-Demand design:  
  https://github.com/simdjson/simdjson/blob/master/doc/ondemand_design.md

- Apple networking guidance:  
  https://developer.apple.com/documentation/technotes/tn3151-choosing-the-right-networking-api

- Apple App Review Guidelines:  
  https://developer.apple.com/app-store/review/guidelines/

---

# 48. Final Project Directive

Crossa must be built as a **compiler-powered native runtime platform**, not as a collection of utilities.

The project must optimize the complete path from backend contract to application-ready object.

For the first milestone, this means:

```text
Backend Contract
      |
      v
Crossa Compiler
      |
      v
Generated Native API Module
      |
      v
C++ Request Encoding
      |
      v
C++ Transport
      |
      v
Native Response Buffer
      |
      v
C++ Generated Parsing
      |
      v
Native Typed Result
      |
      +------------------+
      |                  |
      v                  v
Android Kotlin       iOS Swift
ready object/view    ready object/view
```

Every architectural decision should strengthen one or more of the following:

- Portability.
- Native performance.
- Low memory usage.
- Low allocation count.
- Low copy count.
- Stable interoperability.
- Build-time specialization.
- Backend/mobile consistency.
- Long-term modularity.
- Future protocol expansion.

Networking is the proof of architecture.

Database, WebSocket, raw sockets, binary sockets, streaming, caching, and other modules are future applications of the same compiler/runtime foundation.

**Do not optimize Crossa around the first feature so aggressively that the runtime cannot become the platform.**

**Do not design the language before the platform tells us what the language must express.**

The first objective is to prove that Crossa can compile a real backend contract into Android and iOS SDK artifacts whose request execution, response parsing, memory ownership, and typed results are controlled by a shared high-performance C++ runtime.

Once that foundation is stable, the language can be designed on top of a system that already knows what it needs to represent.
