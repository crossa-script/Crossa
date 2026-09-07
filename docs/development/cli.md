# Crossa CLI

## Commands

Running `crossa` without arguments starts an interactive wizard when standard
input is attached to a terminal. The wizard collects a project root, source,
and supported Android/iOS generation settings, then displays the equivalent
explicit command before dispatching through the normal Crossa application
pipeline. Enter accepts the displayed default; `0` backs out of secondary
screens, `0` at the root exits, and Ctrl+C or EOF cancels cleanly.

Non-TTY invocations never wait for input. They print an actionable diagnostic
and return non-zero. `--no-input` makes this policy explicit: it is accepted
with explicit commands, while `crossa --no-input` fails without prompting.

```text
crossa [check|run|test] [--debug] <file.cra>
crossa run [--project-root <directory>] <file.cra>
crossa generate kotlin [--debug] <file.cra> --output <directory>
crossa generate-build android [--debug] <project-directory> --output <directory> [--entry <file.cra>] [--ndk-version <version>] [--gradle-version <version>] [--kotlin-version <version>]
crossa generate-build ios [--debug] <project-directory> --output <directory> [--entry <file.cra>] [--package-version <version>] [--package-base-url <url>]
crossa --no-input <explicit command>
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
`generate-build ios` writes its generated Xcode framework project under
`<output>/project`, archives Debug and Release device/simulator frameworks, and
creates `<output>/debug/Crossa.xcframework` and
`<output>/release/Crossa.xcframework` when the Apple toolchain and CMake are
available.

Generation prints the exact CLI path, version, source commit, and CLI SHA-256.
The same values are written to each generated artifact manifest. Passing both
package options produces a remote SwiftPM binary target; omitting the base URL
keeps a local path target for development.

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

The interactive Android flow exposes only the currently implemented output:
the generated Android Gradle project used as input for AAR assembly. It uses
the authoritative Android build requirements for Kotlin, NDK, Gradle, and the
single supported `arm64-v8a` ABI. The selected entry is passed as `--entry`;
without that flag the explicit generator retains its existing behavior of
generating every non-configuration source in the project.

The generated Android project currently declares both Debug and Release
variants; because the CLI does not select or build one variant, the wizard
does not show a build-configuration question.

The interactive iOS flow validates only Apple prerequisites and then builds
both Debug and Release XCFramework artifacts. It does not require Android SDK
or NDK checks.

## Doctor

```text
crossa doctor
```

`doctor` inspects the current machine and reports installed Android and iOS
build prerequisites. It does not install dependencies, download files,
modify environment variables, or edit shell configuration.

Doctor output is grouped by Crossa, host, Android, iOS, and storage checks. Each row
uses one status symbol:

```text
✓ Passed
! Warning
✗ Failed
```

Passed checks satisfy the selected platform's build requirements. Warnings report
optional or recommended setup issues that do not block the current build path.
Failed checks report missing required setup and make `crossa doctor` exit
non-zero.

When required checks fail, `doctor` prints a `Fix:` section with actionable
remediation. The Android NDK, CMake, Java, and SDK platform checks use
Crossa's default Android build requirements. Custom version overrides are
evaluated when `crossa generate-build android` writes a project.
