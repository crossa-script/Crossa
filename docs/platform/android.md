# Android AAR Generation

## Status

The CLI now generates the deterministic Android Gradle library project, its
Kotlin configuration surface, Android manifest, and native CMake setup. Native
runtime embedding, JNI configuration application, generated request/model
bindings, and AAR assembly verification remain in progress.

## Command

```text
crossa generate-build android <project-directory> --output <directory>
```

The command discovers the project `.cra` files, compiles them through the
canonical C++ frontend, and writes the Android Gradle library project.
`config.cra` is compiled as configuration and never becomes a Kotlin API class.
The generated project is the input to its Gradle AAR assembly step while the
native runtime embedding work is completed.

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
