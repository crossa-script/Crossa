# Crossa CLI

## Commands

```text
crossa [check|run|test] [--debug] <file.cra>
crossa generate kotlin [--debug] <file.cra> --output <directory>
crossa generate-build android [--debug] <project-directory> --output <directory> [--ndk-version <version>] [--gradle-version <version>] [--kotlin-version <version>]
crossa --version
crossa doctor
```

`check` validates source loading, imports, parsing, semantic analysis, and typed
IR lowering without execution. `run` executes reachable top-level calls through
the native runtime. `test` runs with assertion semantics and exits non-zero on
failed assertions or runtime errors.

`generate kotlin` writes deterministic Kotlin source for pure translated IR.
`generate-build android` writes a generated Android Gradle library project for
AAR assembly.

## Android Build Tool Versions

`generate-build android` accepts version overrides for the generated Android
project. Each option applies only to that command and is written to its
corresponding generated build file:

| Option | Generated location | Default when omitted |
|--------|--------------------|----------------------|
| `--ndk-version <version>` | `library/build.gradle.kts` `ndkVersion` | The recommended installed NDK, or Crossa's minimum when none is found |
| `--gradle-version <version>` | `gradle/wrapper/gradle-wrapper.properties` distribution URL | Crossa's Gradle Wrapper version |
| `--kotlin-version <version>` | Root `build.gradle.kts` Kotlin Android plugin | Crossa's Kotlin Android plugin version |

```text
crossa generate-build android ./crossa-project --output ./build/crossa-aar --ndk-version 28.1.13356709 --gradle-version 8.11.1 --kotlin-version 2.0.21
```

Crossa validates that NDK versions are numeric dotted versions and that Gradle
and Kotlin versions contain only safe version characters. Compatibility among
the selected Gradle, Android Gradle Plugin, Kotlin, and NDK versions remains
the caller's responsibility.

## Doctor

```text
crossa doctor
```

`doctor` inspects the current machine and reports whether Crossa is ready to
build Android AAR artifacts. It does not install dependencies, download files,
modify environment variables, or edit shell configuration.

Doctor output is grouped by Crossa, host, Android, and storage checks. Each row
uses one status symbol:

```text
✓ Passed
! Warning
✗ Failed
```

Passed checks satisfy the current Android build requirements. Warnings report
optional or recommended setup issues that do not block the current build path.
Failed checks report missing required setup and make `crossa doctor` exit
non-zero.

When required checks fail, `doctor` prints a `Fix:` section with actionable
remediation. The Android NDK, CMake, Java, and SDK platform checks use
Crossa's default Android build requirements. Custom version overrides are
evaluated when `crossa generate-build android` writes a project.
