# Android AAR Generation

## Status

The generated Android project embeds the native execution/runtime sources,
creates a `NativeRuntime` through the shared ABI, and registers JNI methods
explicitly from `JNI_OnLoad`. Generated arm64-v8a AARs build the production
native networking stack in both Debug and Release modes.

## Command

```text
crossa generate-build android <project-directory> --output <directory>
```

The command discovers the project `.cra` files, compiles them through the
canonical C++ frontend, and writes the Android Gradle library project.
`config.cra` is compiled as configuration and never becomes a Kotlin API class.
The generated project is the input to its Gradle AAR assembly step while the
native runtime embedding work is completed. It contains the repository-trusted
Gradle Wrapper, including `gradlew`, `gradlew.bat`, and the wrapper JAR and
properties, so a global `gradle` executable is not required.

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

`doctor` checks whether the current machine can build generated Android AAR
artifacts without relying on the Crossa source repository. It reports the
Crossa installation, host operating system and CPU architecture, `ANDROID_HOME`,
required Android SDK platform and build-tools presence, side-by-side NDK,
CMake, Ninja, Java, Crossa cache writability, and temporary directory
writability.

The command uses `✓` for passed checks, `!` for warnings, and `✗` for required
failures. Any required failure makes the command exit non-zero and prints a
`Fix:` section. Warnings do not block the current Android build path.

## Build-Variant Configuration

`config.cra` provides the default values. The generated library has one typed
`CrossaConfigurationOverrides` data class for every Android flavor/build type.
Its nested data classes represent structured policies. The selected override
can replace any supported config key; a null property keeps the default.
Native runtime creation validates the complete merged result before it creates
the network engine or scheduler.

```text
config.cra defaults
    -> selected flavor/build-type override
    -> native type validation
    -> RuntimeConfiguration
    -> native scheduler and NetworkEngine
```

The override document is a build input and must not rewrite `config.cra`.
Values embedded in an AAR are inspectable by applications, so secrets must use
runtime-provided secure storage rather than flavor literals.

## Native Boundary

Kotlin APIs call a small JNI bridge once to create a native runtime with the
merged configuration. Generated APIs expose
`configure(overrides: CrossaConfigurationOverrides)` to apply one typed
in-memory update before subsequent requests. Request invocation, cancellation,
terminal completion, models, and lists use opaque handles. Native C++ owns
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

An accepted operation may be cancelled or released independently of a terminal
result. Retained result/error handles remain readable after the operation
handle is released, but never after `crossaReleaseRuntime`. Runtime release
first shuts down and joins the native scheduler, then destroys the runtime and
invalidates every remaining result/error handle. Bindings must close native
result views before releasing their runtime during normal lifecycle teardown.

ABI accesses acquire a temporary strong `NativeRuntime` reference from the
registry and release the registry mutex before accessing result storage. This
prevents a concurrent runtime release from leaving an ABI call with a dangling
result-context reference.

## Result Views

The shared ABI now defines runtime-owned result handles, list access, root and
list-element model views, and typed scalar field access. A generated model uses
declaration-order field indexes; it does not perform reflection or string field
lookup. `CrossaNativeResult` is the root owner for native-backed Kotlin views
and supports deterministic `close()`. A closed result rejects list and model
access before JNI can reach released native storage.

## 16 KB Page-Size Support

The generated Android project requires AGP 8.5.1+ and NDK r28+. NDK r28 builds
16 KB-aligned ELF files by default. Older supported NDKs must use both
`-Wl,-z,max-page-size=16384` and `-Wl,-z,common-page-size=16384`. CI must
verify every generated arm64-v8a library and package alignment before release.

These requirements follow the Android 16 KB guidance:
[Support 16 KB page sizes](https://developer.android.com/guide/practices/page-sizes).
