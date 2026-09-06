# Android AAR Generation

## Status

The generated Android project embeds the native execution/runtime sources,
creates a `NativeRuntime` through the shared ABI, and registers JNI methods
explicitly from `JNI_OnLoad`. Generated arm64-v8a AARs build the production
native networking stack in both Debug and Release modes.

## Command

```text
crossa generate-build android <project-directory> --output <directory> [--ndk-version <version>] [--gradle-version <version>] [--kotlin-version <version>]
```

The command discovers the project `.cra` files, compiles them through the
canonical C++ frontend, and writes the Android Gradle library project.
`config.cra` is compiled as configuration and never becomes a Kotlin API class.
The generated project is the input to its Gradle AAR assembly step. Native
runtime embedding is included. It contains the repository-trusted
Gradle Wrapper, including `gradlew`, `gradlew.bat`, and the wrapper JAR and
properties, so a global `gradle` executable is not required.

The optional version flags select the exact NDK, Gradle Wrapper distribution,
and Kotlin Android plugin written to that generated project. When omitted,
Crossa uses its current defaults. These overrides apply only to one generated
project and do not modify the Crossa installation or source templates.
Compatibility among user-selected tool versions remains the caller's
responsibility.

## Project-Aware Kotlin Layout

Android generation indexes the complete linked IR once before Kotlin text is
emitted. A model is canonicalized from its validated project declaration and
is written once under `model/`; functions remain owned by their original source
unit and are grouped under `api/`. The generated Kotlin package root therefore
contains deterministic logical directories:

```text
api/<PascalCaseSourceUnit>.kt
model/<Model>.kt
runtime/CrossaState.kt
runtime/CrossaError.kt
internal/CrossaNativeBridge.kt
```

Imports are planned and sorted before writing a file. The generated manifest
tracks only Crossa-owned Kotlin outputs, allowing obsolete generated files to
be removed without deleting unrelated project files. Kotlin remains a thin JNI
view over native result paths. Nested model and list fields stay native-backed.

## Debug and Release AARs

The generated Android library declares both Gradle build types. `debug` uses
the native Debug configuration, keeps Kotlin unminified, and enables JNI
debugging. `release` uses the native Release configuration and enables R8 with
focused library and consumer rules for public API surfaces and the explicitly
registered JNI bridge. The native CMake project does not force `-O3`; the
Android/CMake build configuration selects its normal Debug or Release flags.

## Native Network Dependencies

Generated Android projects provision their own static native dependencies with
the Android NDK for arm64-v8a/API 23. The generated CMake cache verifies each
official source download before extraction and reuses successful downloads and
build output within the Gradle CMake build tree.

- OpenSSL 3.0.15: SHA-256 `23c666d0edf20f14249b3d8f0368acaee9ab585b09e1de82107c66e1f3ec9533`
- curl 8.12.1: SHA-256 `0341f1ed97a26c811abaebd37d62b833956792b7607ea3f15d001613c76de202`
- curl Mozilla CA bundle `cacert-2025-02-25.pem`: SHA-256 `50a6277ec69113f00c5fd45f09e8b97a4b3e32daa35d3a95ab30137a55386cef`

curl is built with the generated static OpenSSL archive and linked with it into
`libcrossa_runtime.so`. The pinned CA bundle is generated as immutable native
bytes and applied centrally with `CURLOPT_CAINFO_BLOB`; peer and hostname
verification remain enabled. Kotlin does not load CA files or own transport
behavior.

## Toolchain Validation

```text
crossa doctor
```

`doctor` checks whether the current machine can build Android AAR artifacts
with Crossa's default tool requirements, without relying on the Crossa source
repository. It reports the
Crossa installation, host operating system and CPU architecture, `ANDROID_HOME`,
required Android SDK platform and build-tools presence, side-by-side NDK,
CMake, Ninja, Java, Crossa cache writability, and temporary directory
writability.

The command uses `✓` for passed checks, `!` for warnings, and `✗` for required
failures. Any required failure makes the command exit non-zero and prints a
`Fix:` section. Warnings do not block the current Android build path.

## Build-Variant Configuration

`config.cra` is compiled into the generated native program and supplies the
default runtime configuration. `CrossaConfigurationOverrides` is a typed
Android-only initialization object; its non-null values are serialized once,
validated in C++, and applied before native runtime creation. It does not
mutate configuration after initialization.

The generated runtime uses the embedded `config.cra` program as its default
configuration source and applies validated initialization overrides before
creating the native runtime. Values embedded in an AAR or supplied as build
variant literals are inspectable by applications, so secrets must use
runtime-provided secure storage rather than flavor literals.

## Native Boundary

Kotlin APIs call a small JNI bridge once to create a native runtime with the
embedded configuration. Generated APIs expose both cancellable callback
operations and `suspend` operations backed by
`suspendCancellableCoroutine`; cancellation reaches the native operation
handle. Request invocation, cancellation, terminal completion, models, and
lists use opaque handles. Native C++ owns
HTTP, retries, serialization, response decoding, state transitions, and
scheduling.

`CrossaNativeBridge` is registered through `RegisterNatives`; no Java-mangled
per-method exports are used. It creates a runtime from the generated
`CrossaGeneratedProgram`, forwards scalar arguments to the shared ABI, retains
asynchronous callbacks with a JNI global reference, and attaches a scheduler
thread only while delivering its terminal callback. The bridge has no
interpreter, scheduler, result registry, or networking implementation.

## Runtime and Result Lifetime

Each ABI runtime handle resolves to one `shared_ptr<NativeRuntime>`. That
`NativeRuntime` owns the sole result/error context for the runtime; the ABI
registry does not create a second result arena. A result or error is therefore
identified by its owning runtime handle plus its local result/error handle.

Runtime shutdown is split into an idempotent request and a worker join. When a
callback running on a Crossa worker releases its runtime, the ABI registry
defers the final runtime reference to its joinable non-worker reaper. This
prevents self-join and prevents destruction of scheduler-owned thread objects
on the worker that is still executing the callback.

An accepted operation may be cancelled or released independently of a terminal
result. Retained result/error handles remain readable after the operation
handle is released, but never after `crossaReleaseRuntime`. Runtime release
first shuts down and joins the native scheduler, then destroys the runtime and
invalidates every remaining result/error handle. Bindings must close native
result views before releasing their runtime during normal lifecycle teardown.

ABI accesses acquire a temporary strong `NativeRuntime` reference from the
registry and release the registry mutex before accessing result storage. This
prevents a concurrent runtime release from leaving an ABI call with a dangling
result-context reference. Terminal operation completion removes execution
ownership independently of the optional operation handle, so ignored platform
operation identifiers do not retain completed execution state.

## Result Views

The shared ABI defines runtime-owned result handles and index-based value paths.
Android JNI forwards those path accessors rather than generating a method per
nested type combination. Generated models, `CrossaNativeList`, and `CrossaJson`
are borrowed views over one `CrossaNativeResult`. Nested `Model` and `List`
fields are supported. Scalar results copy then close the native handle. A closed
result rejects later field, list, and Json access at the Kotlin boundary.

`Json` results are native-backed (`CrossaJson`) through additive ABI v1 path
accessors. Kotlin does not parse JSON with `org.json` or another platform parser.

## 16 KB Page-Size Support

The generated Android project defaults to AGP 8.5.1+ and NDK r28+. NDK r28 builds
16 KB-aligned ELF files by default. Older supported NDKs must use both
`-Wl,-z,max-page-size=16384` and `-Wl,-z,common-page-size=16384`. CI must
verify every generated arm64-v8a library and package alignment before release.

These requirements follow the Android 16 KB guidance:
[Support 16 KB page sizes](https://developer.android.com/guide/practices/page-sizes).
