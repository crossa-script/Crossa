# ADR 0001: Native CrossaRequest Runtime

## Status

Accepted for the foundation implementation. Result-state, cancellation, and
typed-storage details are extended by ADR 0002.

## Context

Crossa needs its first complete native networking path. The language must describe HTTP requests and JSON payloads while C++ owns request construction, transport, response parsing, scheduling, errors, and observability. The implementation must remain bounded and must not create one thread per request.

## Decision

- `CrossaRequest` lowers through AST, typed semantics, and platform-neutral IR.
- The request fields are `url`, `method`, `headers`, `customHeaders`, `queryParams`, `pathVariables`, `body`, and `timeout`. `path` remains a compatibility alias for `url`.
- HTTP methods are `GET`, `POST`, `PUT`, `PATCH`, `DELETE`, `HEAD`, `OPTIONS`, `TRACE`, and `CONNECT`.
- `Json` is an explicit language/runtime type. JSON objects, arrays, strings, numbers, booleans, and null are supported without adding general Crossa maps or nullable semantics.
- A sibling `config.cra` supplies the base URL, common headers, interceptor policy, limits, and scheduler bounds.
- Absolute HTTP/HTTPS URLs bypass `baseUrl`; relative URLs are joined with it.
- libcurl is wrapped by a Crossa-owned transport. Easy handles are pooled and reused on a bounded shared scheduler to preserve connection reuse without one thread per request.
- The initial native JSON parser is bounded and Crossa-owned. Typed model/list responses are schema-decoded into native values against IR. Generated direct decoders remain the production optimization path.
- `@Sync` executes in the calling context. `@Async` schedules and discards the result. `@AsyncAfter` schedules on the shared pool and exposes exactly one `Success`, `Failed`, or `Cancelled` result; the CLI waits for that result while platform bindings can bridge it asynchronously later.

## Alternatives

- Platform networking was rejected because it duplicates semantics and violates C++ hot-path ownership.
- A process per request or the curl command-line tool was rejected because it loses lifecycle, reuse, and cancellation control.
- One thread per request was rejected because it is unbounded.
- A third-party JSON public API was rejected because dependencies must remain internal and the current workspace has no selected JSON package.
- A curl-multi event loop remains a future transport implementation when concurrency benchmarks justify it.

## Consequences

- The executable links libcurl and requires a compatible development package.
- Queue size, worker count, response bytes, JSON depth, and timeouts are bounded.
- JSON DOM materialization is retained only at the explicit `Json` boundary. The foundation decoder uses a bounded temporary DOM that is released after native model/list construction. Known production schemas can replace it with generated direct decoding without changing language or IR semantics.
- Transport, scheduler, parser, and interceptor types remain internal and replaceable.

## Compatibility and Migration

Existing pure `.cra` files remain valid. The former `path` request field is accepted as an alias, but new source should use `url`. Exact mobile dependency pinning and ABI exposure remain separate release decisions.
