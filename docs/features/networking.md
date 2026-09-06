# Native Networking

## Implemented Foundation

`CrossaRequest` is compiled by the canonical C++ frontend and executed by the native runtime. It supports absolute or base-relative URLs, path interpolation, path-variable aliases, query parameters, common and request headers, JSON bodies, standard HTTP methods, request timeouts, bounded response buffering, JSON parsing, and typed response validation.

Request header precedence is:

1. common headers from `config.cra`,
2. request `headers`,
3. request `customHeaders`.

Absolute `http://` and `https://` URLs bypass `baseUrl`. Other values require `baseUrl` and are joined with one slash boundary. Query and path values are percent encoded.

The configured interceptor observes every request. It appends common headers and
can emit debug lifecycle events. `logHeaders` and `logBody` independently
control request and response values and default to `false`. When header logging
is enabled, `excludedHeaders` removes matching names case-insensitively from
both request and response logs. When request logging is enabled, Crossa also
emits one copyable `curl` command built from the final prepared request so the
same request can be replayed outside Crossa. Lifecycle logs strip query strings
for privacy. The copyable `curl` command keeps the final URL, including query
parameters. The emitted `curl` command omits excluded headers and only includes
headers or body content when `logHeaders` or `logBody` allow them. Header and
body values are never logged unless their corresponding options are enabled.

```cra
interceptor: {
    enabled: true,
    logRequests: true,
    logResponses: true,
    logHeaders: true,
    logBody: false,
    excludedHeaders: ["Authorization", "Cookie", "Set-Cookie"]
}
```

Successful `2xx` responses are decoded from the containing function's logical
return type. `String` receives the bounded raw response body; `Int`, `Long`,
`Double`, and `Bool` require matching JSON scalars; `Json` receives any JSON value; model and list
results are decoded recursively against the lowered IR schema. Known models and
lists become immutable native `NativeModel` and `NativeList` values; their
temporary generic JSON DOM is released after construction. Only an explicit
`Json` result retains generic JSON storage. Non-`2xx` responses and malformed or
incompatible JSON produce structured native errors.

`@Sync` executes the request inline. `@Async` queues fire-and-forget work, and
`@AsyncAfter` queues work and preserves one terminal `Success`, `Failed`, or
`Cancelled` result. Every async operation has an idempotent `RequestHandle` that
propagates through libcurl and response decoding. Direct CLI calls wait for
`@AsyncAfter`; generated platform completion bridges remain planned.

The stable error categories distinguish HTTP status, timeout, connection, TLS,
invalid JSON, response type mismatch, cancellation, and internal runtime errors.
HTTP and transport metadata remain attached to `CrossaError`.

## Integrated Network Policies

The native runtime now accepts bounded retry policies, named Bearer
authentication providers, multipart parts from memory or file paths, proxy and
certificate policies, request coalescing, transfer progress metrics, and
structured telemetry. These policies may be supplied globally in `config.cra`
or overridden on an individual `CrossaRequest`.

Retrying is limited to ten attempts, uses bounded exponential backoff, and
defaults to idempotent methods and transient HTTP statuses. Non-idempotent
methods require `retryNonIdempotent: true`. Authentication refresh uses a
refresh-token grant and retries the original request once after a `401`.

Certificate verification remains enabled and cannot be disabled by policy.
Proxy credentials and authentication values are never emitted by structured
telemetry. Multipart uploads use native libcurl MIME parts and can read a
bounded source file. `uploadProgress` records native transfer counters.

`downloadStreaming` records native chunk and progress counters while retaining
the response buffer for the current decoder contract. Incremental delivery to
generated Android/iOS APIs remains a future stream ABI decision.

## Limits

The transport uses pooled reusable libcurl easy handles on the shared bounded
scheduler. It does not create a thread per request. Platform cancellation
bridging and generated direct schema decoders remain planned work.

## Integration Verification

The opt-in integration fixtures use JSONPlaceholder:

```text
tests/network-jsonplaceholder.cra
examples/imports/runPosts.cra
```

They verify native typed model/list decoding, absolute URL handling, request
headers, query parameters, and the imported request/model graph. Run them with
`CROSSA_RUN_NETWORK_INTEGRATION=1 ./test.sh` or enable
`CROSSA_ENABLE_NETWORK_INTEGRATION` in CMake.
