# Runtime Memory Ownership

## Native Result Ownership

`RuntimeValue` owns scalar and explicit `Json` values directly. Typed response models and lists use immutable `NativeModel` and `NativeList` storage. A runtime value holds controlled shared ownership of that immutable storage so values can cross asynchronous completion and future ABI-backed views without copying complete object graphs.

`NativeModel` owns fields in deterministic schema order. `NativeList` owns elements in response order. Fields and elements are native `RuntimeValue` values and never reference the temporary response buffer.

The current response decoder parses into one bounded temporary JSON DOM, recursively constructs native values from the IR schema, then releases the DOM. Only a function whose declared result is `Json` retains generic JSON storage.

## Cancellation Ownership

Every `RequestHandle` owns a small shared cancellation state. The scheduler, execution frame, transport, parser, decoder, and future platform bridge share that state only for the lifetime of one operation. Cancellation does not own the task, response, callback, or runtime.

The scheduler owns queued work and active worker bookkeeping. `ScheduledTask` owns the unique terminal future and a copy of its request handle. Shutdown requests cancellation before joining workers, so active code can release buffers and pooled transport handles through normal stack unwinding.

The transport tracks each checked-out curl handle with its operation handle. Transport destruction stops acquisition, cancels checked-out operations, waits for their handles to return, and only then releases native curl resources.

## Boundary Rules

- Exceptions stay inside C++ implementation boundaries.
- `CrossaState` and `CrossaError` are the terminal native data contract.
- Platform bindings retain native-backed handles for large results and do not eagerly duplicate every model or list element.
- A handle or native view must never outlive the immutable storage it references.
