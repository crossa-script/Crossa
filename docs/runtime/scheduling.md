# Runtime Scheduling

Crossa owns one scheduler for native execution. `@Async` and `@AsyncAfter` submit work to its bounded queue; modules do not create private pools.

The default worker count is derived from available hardware and conservatively capped until mobile benchmarks establish target-specific values. `config.cra` may provide bounded `workerThreads` and `maxQueuedTasks` overrides.

`@Async` is fire-and-forget and reports uncaught failures through the runtime logger. `@AsyncAfter` produces one future terminal result. Nested work already running on a scheduler worker executes inline when waiting would deadlock the bounded pool. Runtime shutdown stops new submissions and joins workers after queued work finishes.
