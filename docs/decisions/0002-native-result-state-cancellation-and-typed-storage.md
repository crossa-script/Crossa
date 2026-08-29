# ADR 0002: Native Result State, Cancellation, and Typed Storage

## Status

Accepted.

## Context

Native async execution needs one stable result contract that preserves failures and cancellation without platform-specific reinterpretation. Long-running requests must be cancellable while queued, executing transport work, decoding JSON, or shutting down. Known model and list responses must also outlive their temporary response buffer without retaining a generic JSON document as their runtime representation.

## Decision

- Native result-producing tasks terminate exactly once as `CrossaState<T>::Success(data)`, `Failed(error)`, or `Cancelled`.
- `CrossaError` carries a stable domain, code, message, retryability, optional HTTP status, and optional native transport code. Timeout and cancellation are distinct.
- `CrossaException` is an internal propagation mechanism only. It must be converted to `CrossaState` before a future ABI or platform boundary.
- Each scheduled async operation owns a `RequestHandle`. Copies share one atomic cancellation signal; `cancel()` is race-safe and idempotent.
- The scheduler propagates the handle through function execution, request construction, libcurl progress observation, JSON parsing, and typed response construction.
- Scheduler shutdown cancels queued and executing handles before workers are joined. Queued work observes cancellation before invoking its body.
- A completed state is immutable. Cancellation requested after terminal completion does not replace the already published state.
- Known model responses become immutable `NativeModel` values and known list responses become immutable `NativeList` values. Their fields and elements are native `RuntimeValue` instances.
- The bounded JSON DOM is temporary input to the current schema-aware decoder and is released after typed construction. Only an explicit `Json` result retains a generic JSON value.
- Immutable model/list storage uses controlled shared ownership so runtime values remain cheap to copy and can later be exposed through native-backed platform handles without eager managed duplication.

## Alternatives

- Treating cancellation as `Failed(CrossaError)` was rejected because callers must distinguish user intent from operational failure without inspecting an error code.
- Throwing raw standard exceptions through futures was rejected because exceptions cannot define the stable ABI contract.
- One cancellation flag per subsystem was rejected because scheduler, transport, and decoder could disagree during races.
- Retaining all typed responses as a generic JSON DOM was rejected because it loses semantic type ownership and forces repeated field interpretation.
- Eager Kotlin or Swift object creation was rejected because it increases copies and boundary crossings.

## Consequences

- Native callers retain `ScheduledTask` or its `RequestHandle` to cancel `@Async` and `@AsyncAfter` work safely.
- Fire-and-forget failures remain visible through structured runtime logs; fire-and-forget cancellation is a debug lifecycle event.
- The current decoder still uses one bounded temporary DOM. A generated direct decoder can replace that parsing implementation without changing `RuntimeValue`, `CrossaState`, or platform semantics.
- Platform bindings must map all three terminal states and preserve every `CrossaError` field.

## Compatibility and Migration

No `.cra` cancellation syntax is added. Existing source remains valid. Generated APIs that previously modeled only success and failure must add a cancellation branch before they are considered compatible with this runtime contract.
