# Crossa Technical Architecture

## 1. Purpose and Authority

This document is Crossa's primary technical architecture and engineering reference. It defines system boundaries, dependency direction, ownership, runtime responsibilities, source organization, and accepted implementation patterns. Every implementation must preserve these rules unless a documented architecture decision record (ADR) changes them.

This is not a roadmap, product pitch, API reference, tutorial, or list of speculative features. `CROSSA_PROJECT_BLUEPRINT.md` remains the broader project specification; this document extracts its enduring technical constitution.

## 2. Architecture Overview

Crossa is a compiler-powered native runtime platform. It converts backend contracts into generated, high-performance Android and iOS SDKs:

```text
Backend Contract
        |
        v
Backend Adapter
        |
        v
Normalized Crossa Contract
        |
        v
Compiler and Semantic Model
        |
        v
Crossa IR
        |
        v
Optimization and Linking
        |
        v
Generated Native C++
        |
        v
Crossa C++ Runtime
        |
        v
Stable Native ABI
      /             \
     v               v
Android             iOS
Kotlin API          Swift API
AAR                 XCFramework
```

Networking is the first production use of this architecture, not the architecture itself. The same runtime core and compiler pipeline must remain capable of supporting Database, WebSocket, raw and binary sockets, Streaming, Cache, Compression, Cryptography, File Transport, Telemetry, and other native modules without turning Networking into their foundation.

## 3. Architectural Goals

### Performance

Crossa minimizes heap allocations, memory copies, temporary objects, platform-boundary crossings, and managed-heap pressure. It favors predictable native ownership, resource reuse where measured, cache-friendly layouts, compile-time specialization, capability pruning, and efficient release binaries.

### Portability

Core compiler and runtime behavior remains portable C++. Platform-specific code is isolated at packaging, ABI, and lifecycle boundaries.

### Maintainability

Performance-sensitive code must remain explicit, readable, and auditable. Low-level complexity is accepted only when measurements show a meaningful benefit.

### Modularity

Runtime modules share runtime services but remain independent of one another. Shared behavior belongs in a deliberate runtime-core abstraction.

### Determinism

Given identical inputs, configuration, toolchain, and dependency versions, compilation and generation must produce equivalent output. Generated paths, metadata, diagnostics, and manifests must be stable and reproducible.

## 4. C++ Owns the Hot Path

The performance-critical execution path is a non-negotiable C++ responsibility. Android and iOS consume Crossa; they are not secondary implementations.

For Networking, native code owns request validation and planning, URL/path/query/header construction, authentication metadata, serialization, transport, connection reuse, cancellation, timeouts, response buffering, parsing, typed model construction, errors, retries, cleanup, scheduling, memory, buffers, and object lifetimes.

The same rule applies to future modules. Kotlin and Swift must not own protocol processing, database decoding, WebSocket or binary-message parsing, response buffers, native scheduling, retry engines, or native object storage.

Prefer:

```text
Network or data bytes
        -> native buffer
        -> generated/native parser
        -> native typed representation
        -> platform-facing view
```

Avoid:

```text
Native buffer
        -> copied C++ string
        -> JNI/Swift copy
        -> platform string
        -> platform parser
        -> duplicate platform model
```

Materialization is allowed only when explicit application ownership or platform ergonomics requires it. It must not be the default for large results.

## 5. Subsystems and Compiler/Runtime Boundary

### Backend Adapters

Adapters inspect an external contract source and emit the neutral Crossa Contract. They discover operations, models, constraints, routes, authentication metadata, and supported validation metadata. They do not generate platform APIs, compile C++, implement transport, or know about JNI or XCFramework packaging.

### Contract, Semantic Model, and IR

The Contract represents backend behavior without framework or platform assumptions. Semantic analysis validates and resolves it. Crossa IR represents types, operations, constraints, serialization plans, errors, ownership hints, and module references. The IR must not be designed around REST-only assumptions because future modules require database operations, streams, and socket messages.

### Compiler, Optimizer, and Linker

The compiler normalizes and validates input, builds the semantic model and IR, applies behavior-preserving optimization, links only required capabilities, and emits generated native code, bindings, and manifests. It must be deterministic, version-aware, cacheable, reproducible, and independent of platform toolchains until generation and packaging stages.

### Generated Native Modules

Generated code specializes runtime behavior for known contracts. It may have machine-oriented structure, but must remain deterministic, valid, debuggable, understandable, efficient to compile, and efficient to execute.

### Runtime and Bindings

The runtime owns execution, memory, scheduling, objects, errors, and shared services. The stable ABI exposes a controlled boundary. Kotlin/JNI and Swift wrappers provide platform-native APIs, lifecycle integration, completion delivery, and cancellation bridging without duplicating native work.

## 6. Dependency Architecture

Allowed dependency direction is one-way:

```text
Backend Adapters
        |
        v
Contract
        |
        v
Compiler and Semantic Model
        |
        v
IR
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
Runtime
        |
        v
Stable ABI
        |
        v
Platform Bindings
```

Runtime modules depend inward:

```text
Network ---------\
Database ---------\
WebSocket ---------> Runtime Core
Socket -----------/
Streaming --------/
```

Forbidden dependencies include:

- `runtime -> Android` or `runtime -> Swift`
- `compiler -> JNI` or `compiler -> Swift`
- `network -> NestJS`
- `database -> network`
- `adapter -> Android` or `adapter -> iOS`
- platform bindings owning parsing or transport behavior

Modules do not depend on one another unless a reviewed shared runtime abstraction makes that dependency explicit. Shared code must not be hidden in generic utility packages.

## 7. Runtime and Module Model

A runtime instance explicitly owns configuration, allocators, bounded buffer resources, scheduler resources, module instances, native objects, logging and metrics bridges, platform callbacks, and shutdown coordination. Initialization and shutdown must be explicit, observable, and safe with in-flight operations.

Every module defines:

1. Its public capability contract and internal implementation.
2. Required runtime services and compiler/IR support.
3. Memory, concurrency, cancellation, and error behavior.
4. Observability events and platform integration.
5. Version/capability identifiers and binary-size impact.

A module must not mutate unrelated runtime state or create a private runtime inside the shared runtime.

## 8. C++ Engineering Model

Use C++20 unless the checked-in toolchain establishes another version. Prefer a conservative production subset:

- RAII, deterministic destruction, value semantics, and stack storage where suitable.
- Explicit ownership, `std::unique_ptr` for exclusive dynamic ownership, and minimal shared ownership.
- Move semantics, `const`, scoped enums, strong domain types, and explicit constructors where conversions are dangerous.
- `constexpr` for genuine compile-time values and `noexcept` only when the semantic guarantee is true.
- `std::span` for safe contiguous views and `std::string_view` only when backing lifetime is guaranteed.

Avoid raw owning pointers, uncontrolled `std::shared_ptr`, hidden static initialization, mutable global state, runtime reflection, unnecessary allocation, excessive macros or templates, undefined-behavior-dependent optimization, and exceptions crossing the ABI.

### Simplicity and OOP

Choose the simplest design that is correct, explicit, efficient, and maintainable. An abstraction must solve a current architectural problem. Do not add generic factories, inheritance, interfaces, plugin systems, dependency-injection infrastructure, wrappers, or template frameworks for hypothetical requirements.

Use object-oriented design when an object represents meaningful state, ownership, lifecycle, or behavior. Keep classes focused and prefer composition. Use inheritance only for a genuine polymorphic relationship. Avoid manager classes that combine encoding, parsing, transport, retry, scheduling, authentication, memory, and logging.

## 9. Source and File Organization

Directories and namespaces communicate ownership. Organize by subsystem and responsibility rather than placing all types in one package:

```text
runtime/
    core/
    memory/
    buffers/
    scheduler/
    objects/
    errors/
    observability/

compiler/
    frontend/
    semantic/
    ir/
    optimizer/
    linker/
    generators/
    diagnostics/

modules/network/
    request/
    response/
    transport/
    encoding/
    parsing/
    models/
    errors/
```

Do not create deep directories without a meaningful boundary.

Each meaningful class, struct, enum, or independent domain type normally receives a focused file, with declarations and implementation separated where appropriate. Do not group unrelated types in `CommonTypes.h`, `Models.h`, `Everything.h`, or similar dumping grounds. Small, tightly coupled implementation-only types may remain local to a `.cpp` file when separation would reduce clarity.

An illustrative Networking layout is:

```text
modules/network/
├── NetworkEngine.h
├── NetworkEngine.cpp
├── request/
│   ├── Request.h
│   ├── RequestBuilder.h
│   ├── RequestBuilder.cpp
│   ├── RequestEncoder.h
│   └── RequestEncoder.cpp
├── response/
│   ├── Response.h
│   ├── ResponseParser.h
│   └── ResponseParser.cpp
├── transport/
│   ├── Transport.h
│   ├── CurlTransport.h
│   └── CurlTransport.cpp
└── utils/
    ├── UrlUtils.h
    └── UrlUtils.cpp
```

This communicates responsibility; it is not a template to reproduce with empty abstractions.

## 10. Functions, Utilities, Comments, and Namespaces

Functions perform one logical operation and have names that expose intent. Always decompose workflows into small, meaningful functions so the result remains human-readable. Do not split code only to reduce line counts. Use early returns when they reduce nesting.

Shared stateless operations must be static members of a focused utility class such as `UrlUtils`, `BufferUtils`, `PathUtils`, `HashUtils`, or `PrintUtils`. Do not declare free utility functions or static variables at namespace or global scope; every function and variable must have a clear parent class. Do not create one universal `Utils` class containing unrelated behavior. If behavior naturally belongs to an existing domain class, keep it there.

Comments are short and explain responsibility, a non-obvious invariant, lifetime, platform constraint, ABI rule, or measured performance decision. Function comments belong immediately above the function declaration or definition, never inside the function body. Do not narrate visible implementation steps.

All production C++ lives under `crossa` and an appropriate subsystem namespace, such as `crossa::runtime`, `crossa::compiler`, or `crossa::network`. Never place Crossa types in the global namespace. In `.cpp` files, use `using namespace std;` when standard-library names are required instead of repeatedly qualifying them. Never place `using namespace std;` in a public header.

Use lower camel case for variables, such as `responseBuffer`. Static variables use Pascal case, such as `DefaultCapacity`. Do not place static variables at namespace or global scope; they must belong to a class.

Application code must not call `std::cout` directly. Route output through `PrintUtils::println(string)`. `PrintUtils::println` is the only function allowed to call `std::cout`, keeping console behavior centralized and replaceable.

Headers must be self-contained and include their direct requirements. Do not depend on transitive includes. Order includes as corresponding header, standard/system headers, third-party headers, then Crossa headers. Use forward declarations only when they reduce coupling without obscuring ownership.

Representative style:

```cpp
#pragma once

#include "crossa/network/PreparedRequest.h"
#include "crossa/network/RequestPlan.h"

namespace crossa::network {

class RequestBuilder final {
public:
    // Creates a prepared native request from a compiled operation plan.
    [[nodiscard]] PreparedRequest Build(const RequestPlan& plan) const;

private:
    // Applies encoded path parameters to the request URL.
    static void ApplyPathParameters(
        const RequestPlan& plan,
        PreparedRequest& request
    );

    // Applies encoded query parameters to the request URL.
    static void ApplyQueryParameters(
        const RequestPlan& plan,
        PreparedRequest& request
    );
};

}
```

The example demonstrates focused responsibility, small functions, file separation, concise function comments, and namespace isolation. It is not a required concrete API.

## 11. Memory, Allocation, and Buffers

Memory is designed around explicit lifetimes:

```text
Runtime Lifetime
Operation / Request Lifetime
Response Lifetime
Stream / Socket Lifetime
Temporary / Scratch Lifetime
```

Use stack/value storage where sufficient, move ownership instead of copying, reuse buffers when measured, and use arenas or monotonic allocation for grouped lifetimes when evidence supports them. Pools must remain bounded and must not retain mobile peak memory indefinitely.

Every `std::span`, `std::string_view`, non-owning pointer, handle, and buffer slice has a documented owner and must not outlive its storage. Cancellation and destruction must release resources predictably.

Avoid per-field and per-item heap allocation in hot parsing paths, repeated temporary strings, and generic intermediate object trees when the schema is known. Prefer direct schema-specific parsing into typed native storage.

Before copying data, identify the current owner, required next owner, view safety, required lifetime, and whether materialization is necessary. Buffers require explicit capacity, bounded growth, cheap views, ownership tracking, and cancellation-safe destruction.

Exact object layouts, string/list representations, arena sizes, and pool policies remain benchmark-driven decisions rather than architectural constants.

## 12. Compile-Time Specialization and Generated Code

Prefer generated serializers and parsers, static operation identifiers, generated request/response plans, precomputed metadata, dead-code elimination, constant pooling, capability pruning, and link-time removal over runtime reflection or discovery.

Avoid dynamic field maps, runtime annotation scanning, repeated string-based operation lookup, and generic serialization when the contract enables specialized generation. Optimization must preserve observable contract behavior.

Generated output must be stable for identical builds and must not add runtime work merely because code generation makes that work easy to emit.

## 13. Stable ABI and Errors

The public native boundary is deliberately small and may use a narrow C-compatible ABI even when internals use modern C++. Prefer opaque handles, explicit create/release operations, versioned structures, stable integer error codes, safe buffer access, bulk operations, and capability/version negotiation.

Never expose STL containers, internal classes, inheritance trees, allocators, RTTI assumptions, or compiler-specific layout as durable ABI. Exceptions never cross the boundary.

Use one structured error model across modules. Errors carry a stable domain and code plus message, retryability/recoverability, and optional metadata. Platform bindings map these errors into ergonomic Kotlin and Swift forms without discarding the native cause. Never swallow native errors; cancellation and timeout remain distinct and deterministic.

ABI, Contract schema, IR, runtime implementation, module, compiler, adapter, and generated SDK versions are separate axes. Do not collapse them into one version.

## 14. Android and iOS Boundaries

### Android

The AAR contains the Kotlin API, concentrated JNI bridge, native libraries, and required metadata. Minimize JNI calls, marshalled objects, strings, collections, lookup, and native thread attachment. Never cross once per field. Prefer coarse operations and native-backed handles or indexed views for large results. Kotlin coroutines provide API ergonomics and bridge native completion/cancellation; they do not execute the network pipeline.

### iOS

The XCFramework exposes a safe Swift-facing API over the controlled native boundary. Do not expose the entire internal C++ model or blindly convert large STL collections. Prefer native-backed access and explicit materialization. Direct Swift/C++ interoperability is acceptable only with stable ownership and a measured benefit; the exact C ABI versus direct interop split remains open.

Both platforms must respect lifecycle transitions, background constraints, and main-thread safety without taking ownership of native execution.

## 15. Scheduler, Concurrency, and Global State

Crossa uses one coordinated scheduling architecture for network events, parsing, database work, compression, cryptography, streaming, and platform delivery. Modules do not create independent thread pools.

Avoid one thread per operation, unlimited workers, unbounded queues, blocking event loops, and large parsing work on a UI thread. Mutable runtime state has an explicit owner and synchronization policy. Prefer immutable generated metadata. Use atomics only with clear semantics and keep locks and observability off hot contention paths.

Every long-running operation has explicit state and supports race-safe, idempotent cancellation while queued, executing, or shutting down. Destruction coordinates in-flight work. Timeouts and deadlines propagate through the native lifecycle.

Process-global mutable application state and expensive hidden global initialization are forbidden. State belongs to a Runtime, Module, Operation, Request, Response, Arena, or Connection. Immutable generated static metadata is acceptable only as a member of a clear parent class.

## 16. Performance Engineering

Performance work follows this order:

1. Better algorithms and data flow.
2. Fewer allocations, copies, and temporary objects.
3. Clearer ownership and better data layout.
4. Resource reuse and fewer boundary crossings.
5. Compile-time specialization and capability pruning.
6. Cache-local improvements.
7. Low-level optimization only after measurement.

Measure end-to-end release behavior: latency, CPU, memory, allocations, copies, synchronization, boundary activity, binary size, initialization, sustained load, and energy where practical. Use realistic Android and iOS ARM64 devices before establishing performance claims or thresholds. Record toolchain, device, configuration, and payload characteristics.

No performance assumption justifies unreadable or unsafe code. Exact worker counts, buffer sizes, parser pools, native model layout, LTO configuration, TLS backend, and protocol-stack choices remain open until benchmarks and production requirements decide them.

## 17. Third-Party Dependencies

Dependencies remain behind Crossa-owned abstractions and never define the public architecture. Evaluate performance, memory, binary size, mobile support, security history, maintenance, license, toolchain compatibility, ABI behavior, and upgrade cost.

Use the standard library for small, safe functionality, but do not reimplement mature TLS, protocol, cryptographic, or compression infrastructure merely to avoid a dependency. libcurl and simdjson are current candidates, not public APIs or irrevocable choices.

## 18. Accepted and Forbidden Patterns

Accepted patterns include focused domain objects, explicit runtime instances, native-backed views, schema-specific generated code, bounded resource ownership, composition, narrow interfaces backed by real variation, and measurable optimization.

Forbidden patterns include platform-side parsing or transport, reflection in generated hot paths, generic intermediate object graphs, eager duplication of large results, unbounded resources, independent module schedulers, mutable globals, cross-module coupling, ABI leakage, universal utility classes, unrelated type collections, manager classes with broad responsibility, and speculative abstraction systems.

## 19. Architecture Change Process

Do not introduce a new architectural pattern silently. Create or update an ADR under `docs/decisions/` for changes to ABI, IR, ownership, scheduler, transport, parser, native object or memory layout, allocation strategy, platform interoperability, module boundaries, or major dependencies.

An ADR states context, decision, alternatives, consequences, compatibility and migration impact, and supporting measurements where applicable. Decisions explicitly reserved for benchmarking must not be locked by documentation or incidental implementation.

## 20. Architectural Non-Negotiables

1. C++ owns the performance-critical path.
2. Android and iOS bindings remain thin.
3. Compile-time specialization is preferred over runtime reflection.
4. Ownership and lifetimes are explicit.
5. Memory growth, queues, caches, buffers, and pools are bounded.
6. Cross-boundary calls, copies, and object materialization are minimized.
7. Runtime modules depend on runtime core, not on one another.
8. The public ABI remains small, stable, explicit, and versioned.
9. Exceptions never cross ABI boundaries.
10. Process-global mutable state and independent module thread pools are forbidden.
11. Heap allocation and intermediate representations are avoided when unnecessary.
12. Architecture remains simple, readable, and based on current requirements.
13. Meaningful types live in focused files and coherent subsystem directories.
14. Functions perform one logical operation.
15. Comments describe responsibility or constraints and do not narrate code.
16. Performance decisions require measurements.
17. Generated output is deterministic.
18. Crossa remains extensible beyond Networking.
19. Crossa IR precedes Crossa source-language syntax.
20. Material architecture changes require an ADR.

Crossa performs as much useful work as possible inside the C++ runtime while exposing the smallest safe and ergonomic surface required by Android and iOS. Its performance comes primarily from architecture, ownership, generated specialization, bounded resources, fewer allocations, fewer copies, and fewer boundary crossings—not unnecessary complexity.
