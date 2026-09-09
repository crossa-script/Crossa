# Crossa V0 Current-State Audit

Audit date: 2026-09-09 (Asia/Amman)

The canonical closure result is [the V0 closure report](../development/v0-closure-report.md) and its [machine-readable manifest](../development/v0-closure-manifest.json).

## Current verdict

**CROSSA V0: CLOSED**

The native suite, direct typed decoder, generated Android/iOS Release artifacts, Release consumers, current mobile simulator smoke runs, website checks, Pages synchronization, and final native sanitizer gates passed. The earlier local macOS AppleClang environment could not initialize ASAN/UBSAN before `main`; authoritative GitHub CI subsequently validated both sanitizer jobs for released commit `38e31ae42426be76c34d14ca1ec229a323a979ad`.

## Retained pre-release artifact evidence

These hashes belong to the earlier closure evidence at source commit `8c362e10fba65cbc2279f08c06eb5469e16a72e5`; they are not attributed to `v0.0.13`.

| Artifact | Source | Digest |
|---|---|---|
| CLI | `8c362e10fba65cbc2279f08c06eb5469e16a72e5` | `6dbd4978a41ec2ba1b11370ab6d97f7995f494d71e1b515fd656af5479e042b1` |
| Android Release AAR | same source, ABI 1 | `297b6e05f5dd5cba091a7a2b7c3d14510064ba474d9a5bc18c74027642cf716c` |
| iOS Release XCFramework ZIP | same source, ABI 1 | `4651422f992c5250bc7612b18709728be3905f555845bca6cb97acaa8d53526a` |

## Mobile evidence

Android Release ran on `Google sdk_gphone16k_arm64`, Android 17, `arm64-v8a`. Crossa, Retrofit, and Ktor each completed 8/8 warm and 8/8 cold samples. The retained raw result files are under `android-example/benchmark-results/` and include the earlier AAR digest.

iOS Release ran on the iPhone 14 Pro Max simulator with iOS 17.5 and ARM64. Crossa and Alamofire each completed 8/8 warm samples against JSONPlaceholder after the final decoder changes. The iOS simulator service was unavailable when a new cold run was attempted, so no current-artifact iOS cold result is claimed. These are simulator and remote-network observations, not physical-device performance evidence. Older retained iOS result files are not treated as current-artifact benchmark evidence because their embedded checksum predates the current XCFramework.

## Security evidence

Security scan `94bda93f-15f4-4442-99b0-869e279f51ba` completed with zero reported findings and partial coverage. Delegated workers were unavailable, so discovery and validation were deferred. This is a documented partial result, not a clean full security audit.

## Closure boundary

V0 closure covers the released source and validated gates above. Physical-device validation, remote SwiftPM publication, V1 work, and post-V0 streaming remain outside this closure.
