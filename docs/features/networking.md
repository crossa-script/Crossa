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
both request and response logs. Header and body values are never logged unless
their corresponding options are enabled.

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

## Limits

The foundation transport uses pooled reusable libcurl easy handles on the shared bounded scheduler. It does not create a thread per request. Retries, multipart, streaming, downloads, platform cancellation bridging, and generated direct schema decoders remain planned work.

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
