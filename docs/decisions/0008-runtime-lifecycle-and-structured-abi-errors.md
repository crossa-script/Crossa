# ADR 0008: Runtime Lifecycle and Structured ABI Error Delivery

## Status

Accepted.

## Context

The scheduler previously combined shutdown signaling and worker joining. A
callback running on a scheduler worker could therefore request runtime release
and cause the scheduler to join its own thread while runtime-owned callback,
result, and error state was still executing. Completed operation bookkeeping
also remained in the runtime until an external release. Android completion
mapping discarded native error domain, code, and retryability.

## Decision

- `TaskScheduler` separates idempotent shutdown request from termination wait.
  Workers request shutdown and return to their loop; only a non-worker joins,
  and concurrent joiners wait for the same termination state.
- `NativeRuntime` uses `Running`, `ShutdownRequested`, and `Stopped` states.
  Shutdown rejects new registered operations, cancels registered handles, and
  waits for scheduler termination before runtime destruction.
- Registered operation state is runtime-scoped and removed at terminal
  completion. Scheduler-owned task copies keep execution state alive until the
  worker returns, while result and error handles remain independently owned by
  the runtime context.
- The existing ABI runtime registry defers a worker-originated final runtime
  release to one joinable non-worker reaper. Workers are never detached and no
  second scheduler is created.
- The stable ABI error metadata accessor remains bulk and exposes the existing
  numeric domain, code, and retryable fields. Generated Kotlin reads those
  fields before releasing the error handle instead of manufacturing zero-valued
  metadata.
- Android scalar result mapping closes its native result immediately; model and
  list views retain their owning result until their explicit close.

## Alternatives

- Joining from every caller was rejected because a worker callback can be the
  caller.
- Detaching the worker was rejected because it abandons runtime ownership.
- A process-wide operation/result registry was rejected because runtime-scoped
  ownership already exists.
- Retaining completed operations until platform release was rejected because
  generated callers may ignore the internal handle and retain execution state.
- Separate JNI calls for each error property were rejected in favor of the
  existing bulk ABI accessor and one packed JNI metadata result.

## Consequences

Shutdown from a worker is non-blocking for that worker, while a safe owner still
performs the required joins. A runtime released by an Android callback may
remain briefly in the registry reaper queue until all worker execution ends.
ABI operation release after terminal cleanup is a deterministic invalid-handle
result. The ABI numeric error enums are explicitly assigned and Kotlin now
preserves domain, code, message, and retryability.

## Compatibility and Migration

The C ABI version remains 1; the existing bulk error metadata function is used
without changing its signature. Generated Android sources gain one internal JNI
metadata accessor and synchronized idempotent runtime closing. No `.cra` syntax
or execution semantics change.
