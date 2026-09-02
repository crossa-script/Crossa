# ADR 0009: iOS XCFramework Stable ABI and Native Result Paths

## Status

Accepted.

## Context

iOS must consume the canonical C++ runtime without a second parser, scheduler, networking stack, or eager Swift model graph. Android ABI v1 could expose root models and root list elements but could not represent nested model/list values without introducing raw object pointers or a separate native view registry.

## Decision

- iOS Swift code consumes the existing versioned C ABI and creates generated runtimes through `CrossaAbiRuntimeFactory`.
- ABI v1 gains additive value-path accessors. A path consists only of generated field indexes and list indexes and is evaluated synchronously against one retained root result.
- A Swift native result owner retains exactly one result handle. Generated models and `CrossaList` store value paths and borrow that owner; neither exposes handles in public API.
- Async callback contexts are retained once before native invocation and consumed once by the terminal callback. Immediate ABI invocation failure releases that same context.
- Generated Xcode framework projects build device and simulator archives separately and create the XCFramework through `xcodebuild -create-xcframework`.
- The iOS dependency build provisions the existing pinned curl source as a static library using Apple Security TLS. C++ remains responsible for request execution and response processing.

## Consequences

Nested values remain lazy and schema-indexed. The stable ABI stays free of C++ classes, STL, and raw native addresses. Existing Android callers remain ABI compatible because the additions do not change prior signatures. iOS requires Xcode, the iOS SDK, Swift/Clang, and CMake for dependency provisioning.
