# Native Networking

## Implemented Foundation

`CrossaRequest` is compiled by the canonical C++ frontend and executed by the native runtime. It supports absolute or base-relative URLs, path interpolation, path-variable aliases, query parameters, common and request headers, JSON bodies, standard HTTP methods, request timeouts, bounded response buffering, JSON parsing, and typed response validation.

Request header precedence is:

1. common headers from `config.cra`,
2. request `headers`,
3. request `customHeaders`.

Absolute `http://` and `https://` URLs bypass `baseUrl`. Other values require `baseUrl` and are joined with one slash boundary. Query and path values are percent encoded.

The configured interceptor observes every request. It appends common headers and can emit privacy-aware debug lifecycle events without logging header values or response bodies.

Successful `2xx` responses are decoded from the containing function's logical
return type. `String` receives the bounded raw response body; `Int` and `Bool`
require matching JSON scalars; `Json` receives any JSON value; model and list
results are validated recursively against the lowered IR schema. Non-`2xx`
responses and malformed or incompatible JSON produce native execution errors.

`@Sync` executes the request inline. `@Async` queues fire-and-forget work, and
`@AsyncAfter` queues work and preserves one terminal result. Direct CLI calls
wait for `@AsyncAfter`; generated platform completion bridges remain planned.

## Limits

The foundation transport uses pooled reusable libcurl easy handles on the shared bounded scheduler. It does not create a thread per request. Cancellation handles, retries, multipart, streaming, downloads, and generated direct schema decoders remain planned work.
