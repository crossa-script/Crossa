# ADR 0006: Android AAR Native Runtime and Build-Variant Overrides

## Status

Accepted for the Android AAR implementation.

## Context

Crossa must package generated Kotlin APIs, native runtime code, and compiled
`.cra` behavior as an Android AAR. Android build variants need different
configuration values without modifying source-controlled `config.cra`. The
native runtime must continue to own request execution, response decoding,
scheduling, and completion state.

## Decision

- `crossa generate-build android <project-directory> --output <directory>`
  discovers every `.cra` source below the project directory, excludes
  `config.cra` from generated API source, and compiles the project through the
  canonical C++ frontend.
- The generated output is a self-contained Android library Gradle project. Its
  AAR contains generated Kotlin APIs, a concentrated JNI bridge, generated
  native metadata, and native runtime libraries for the selected ABIs.
- `config.cra` provides immutable default configuration. Android product
  flavors and build types supply an override document that is merged after the
  defaults and before native runtime creation. Every override is type-checked
  against the compiled config key and is rejected when unknown or invalid.
- Flavor override documents may override every supported `config.cra` key.
  They are build inputs, not `.cra` source mutations. The precedence is
  `config.cra` followed by the selected Android flavor/build-type document.
- Kotlin exposes a generated `configure(overrides)` function whose parameter is
  a typed `CrossaConfigurationOverrides` data class with nested generated data
  classes for structured config values. Null properties preserve `config.cra`
  defaults. JNI receives one typed override object and Kotlin does not parse or
  execute `.cra`.
- The public native ABI uses opaque runtime, invocation, result, and
  native-object handles. It uses versioned C structures and error values; no
  C++ exception or STL type crosses JNI.
- `@AsyncAfter` completion and cancellation cross JNI through one native
  operation handle. Kotlin may adapt this to coroutines, but does not create a
  second scheduler or execute HTTP work.
- Generated Android CMake uses NDK r28 or newer. It verifies 16 KB ELF support
  and uses the explicit 16 KB linker flags when an older supported NDK is
  selected. Android Gradle Plugin 8.5.1 or newer packages uncompressed native
  libraries with 16 KB ZIP alignment.
- Release performance defaults favor measured C++ optimization, capability
  pruning, connection reuse, bounded scheduling, and coarse JNI calls. Exact
  compiler flags, worker counts, and buffer sizes remain benchmark decisions.
- Generated projects copy the repository-trusted Gradle Wrapper (Gradle 9.3.0)
  and make `gradlew` executable. The wrapper remains the sole generated Gradle
  version source.
- Generated arm64-v8a projects provision OpenSSL 3.0.15 and curl 8.12.1 from
  their official source archives. The generated CMake file verifies SHA-256
  before extraction: OpenSSL
  `23c666d0edf20f14249b3d8f0368acaee9ab585b09e1de82107c66e1f3ec9533`
  and curl
  `0341f1ed97a26c811abaebd37d62b833956792b7607ea3f15d001613c76de202`.
  Both libraries are built as PIC static archives for Android API 23 and
  linked into `libcrossa_runtime.so`.
- The generated runtime downloads the curl-maintained Mozilla CA bundle
  `cacert-2025-02-25.pem`, verifies SHA-256
  `50a6277ec69113f00c5fd45f09e8b97a4b3e32daa35d3a95ab30137a55386cef`,
  compiles its bytes into the native runtime, and configures curl with
  `CURLOPT_CAINFO_BLOB`. Peer and host verification remain enabled.

## Alternatives

- Rewriting `config.cra` per flavor was rejected because it breaks source
  reproducibility and can cause generated/native configuration drift.
- Kotlin-side HTTP or coroutine-only networking was rejected because it
  duplicates transport, parsing, retry, and scheduler behavior owned by C++.
- Passing one JNI value per configuration key was rejected because it expands
  boundary crossings and makes nested policy overrides inconsistent.
- Treating 16 KB support as a packaging-only setting was rejected because ELF
  segment alignment and every prebuilt native dependency must also be valid.

## Consequences

- Flavor values become part of the Android artifact and must not contain
  secrets that cannot safely live in an APK/AAR.
- Android AAR generation is a distinct native-binding backend, not an
  extension of the pure Kotlin generator.
- CI must build the generated AAR and verify both ELF and package alignment on
  arm64-v8a before publishing an Android artifact.
- Performance claims require Android ARM64 benchmark results for release
  builds; compiler flags alone are not evidence of better request latency or
  threading behavior.
- Generated AARs do not expose libcurl, libssl, or libcrypto as runtime shared
  library dependencies. The sole advertised ABI is arm64-v8a until another ABI
  has equivalent verified dependency provisioning and AAR inspection.

## Compatibility and Migration

Existing `crossa generate kotlin` remains a pure Kotlin source generator.
Existing CLI execution reads only `config.cra` defaults. Android AAR builds add
their selected flavor overrides at native runtime initialization and do not
change `.cra` language syntax.
