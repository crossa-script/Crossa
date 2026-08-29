# ADR 0004: Native Network Policy Integration

## Status

Accepted for the current foundation implementation.

## Context

Crossa needs one configuration and request contract for reliability,
authentication, transfer behavior, network security, deduplication, and
observability. The language does not yet expose callback or stream types, so
these capabilities must remain native and deterministic without introducing a
second platform networking implementation.

## Decision

- `config.cra` owns global retry, authentication-provider, progress, streaming,
  coalescing, proxy, certificate, and telemetry policy.
- `CrossaRequest` may override those policies through typed fields that lower
  through semantic analysis and platform-neutral IR.
- Retry and coalescing execute in `NetworkEngine`; authentication refresh,
  proxy, certificate, and multipart encoding execute through the native
  libcurl transport.
- Certificate peer and host verification remain enabled. Pinning and custom
  CA/client certificate material are opt-in additions to the secure default.
- Coalesced operations share one native response or exception. A waiting
  caller retains independent cancellation while the owner controls transport
  cancellation.
- Native transfer metrics are attached to `HttpResponse` and emitted through
  structured telemetry. Download streaming is currently instrumented and
  buffered because no stable incremental stream ABI exists yet.

## Consequences

Existing `.cra` requests remain valid. New policy fields are validated before
execution, and malformed policy objects fail as runtime configuration or
request errors. Multipart `filePath` values are read by native transport and
must be treated as trusted application input. Token and proxy secrets remain
in native request state and are excluded from telemetry values.
