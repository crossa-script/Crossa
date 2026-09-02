# Runtime Scheduling

Crossa owns one scheduler for native execution. `@Async` and `@AsyncAfter` submit work to its bounded queue; modules do not create private pools.

The default worker count is derived from available hardware and conservatively capped until mobile benchmarks establish target-specific values. `config.cra` may provide bounded `workerThreads` and `maxQueuedTasks` overrides.

`@Async` is fire-and-forget and reports structured uncaught failures through the runtime logger. `@AsyncAfter` produces exactly one `CrossaState<T>` terminal result: `Success(data)`, `Failed(error)`, or `Cancelled`. Nested work already running on a scheduler worker executes inline with the current request handle when waiting would deadlock the bounded pool.

Every queued operation has a `RequestHandle`. Its shared cancellation signal is race-safe and idempotent. Cancellation before execution skips the task body; cancellation during execution propagates through the execution frame into transport and decoding; cancellation after terminal publication does not alter the published state.

Platform bridges retain a separate opaque operation identifier only for explicit
cancel/release lifecycle control. `NativeRuntime` maps that identifier to the
underlying `RequestHandle`; it never exposes a scheduler pointer or a C++
request object across the ABI. An operation identifier is not reused, and its
release drops only bridge bookkeeping, never result ownership.

Runtime shutdown stops new submissions, cancels queued and active handles, drains the now-cancelled queue, and joins every worker from a non-worker owner. A worker may request shutdown, but never waits for or joins itself. Cancellation and timeout are separate terminal causes.

The scheduler exposes separate shutdown-request and termination-wait phases. The
request phase is idempotent and wakes every worker. The wait phase has one
joining owner; concurrent non-worker callers wait for the same termination
publication. Runtime operation records are removed when their terminal state is
published, while the scheduler's copied `RequestHandle` keeps execution safe
until the worker returns.
