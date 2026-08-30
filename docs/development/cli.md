# Crossa CLI

## Commands

```text
crossa [check|run|test] [--debug] <file.cra>
crossa generate kotlin [--debug] <file.cra> --output <directory>
crossa generate-build android [--debug] <project-directory> --output <directory>
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
remediation. The Android NDK, CMake, Java, and SDK platform requirements come
from the same generated Android build configuration used by
`crossa generate-build android`.
