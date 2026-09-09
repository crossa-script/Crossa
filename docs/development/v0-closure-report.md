# Crossa V0 Closure Report

Date: 2026-09-09 (Asia/Amman)

Final V0 release: `v0.0.13`

Final V0 source commit: `38e31ae42426be76c34d14ca1ec229a323a979ad`

The GitHub release/tag version is `v0.0.13`. The compiler/CLI reports internal version `0.1.0`, the CRA language version is V0, and the stable runtime ABI is `1`. These are separate version axes. The source state recorded here is the immutable release commit; it does not assert that the current local workspace is clean.

## Verdict

**CROSSA V0: CLOSED**

Repository-controlled native, generator, artifact, consumer, mobile Release smoke, website, and final native sanitizer gates passed. The earlier local macOS environment could not initialize AppleClang ASAN/UBSAN before `main`; the final V0 commit was subsequently validated by the authoritative GitHub CI sanitizer jobs. The local environment limitation therefore no longer blocks V0 closure.

The security scan completed with zero reported findings but partial coverage: delegated workers were unavailable and discovery/validation did not execute. It must not be interpreted as a clean full security audit.

## Validation

| Area | Result | Evidence |
|---|---|---|
| Native tests | PASS | `./test.sh`; language, runtime, decoder, and CLI fixture tests exit 0 |
| Typed response decoding | PASS | Direct schema-aware scalar/model/list decoding, unknown-field skipping, nested explicit `Json`, malformed/duplicate/type-mismatch cases |
| Kotlin and Android generators | PASS | Generator test suites and fresh Android Release generation |
| Android Release consumer | PASS | `./gradlew verifyDemo :app:assembleRelease` |
| Android Release runtime | PASS | `Pixel_10_Pro`, Android 17, ARM64; Crossa, Retrofit, and Ktor each completed 8/8 warm and 8/8 cold samples |
| iOS Release generation | PASS | Fresh XCFramework with device and simulator ARM64 slices |
| iOS Debug/Release consumer | PASS | `xcodebuild` Debug and Release builds |
| iOS Release runtime | PASS — warm smoke | iPhone 14 Pro Max simulator, iOS 17.5; Crossa and Alamofire each completed 8/8 warm samples |
| Website | PASS | `pnpm typecheck`, `pnpm lint`, `pnpm build`; 34 SEO routes generated |
| Website synchronization | PASS | `website/scripts/build-and-deploy.sh` synchronized `website/dist` into `crossa-script.github.io` |
| GitHub build-and-test | PASS | GitHub `Crossa CI #46`, run `34399392935`, commit `38e31ae42426be76c34d14ca1ec229a323a979ad`, job `build-and-test` — Success |
| ASAN | PASS | GitHub `Crossa CI #46`, run `34399392935`, commit `38e31ae42426be76c34d14ca1ec229a323a979ad`, job `asan` — Success |
| UBSAN | PASS | GitHub `Crossa CI #46`, run `34399392935`, commit `38e31ae42426be76c34d14ca1ec229a323a979ad`, job `ubsan` — Success |
| Security scan | PARTIAL | Completed scan `94bda93f-15f4-4442-99b0-869e279f51ba`; zero reported findings, six surfaces deferred because worker capacity was unavailable |

Mobile timings are simulator plus remote-network observations, not physical-device performance claims. Android warm/cold runs are retained pre-release artifact evidence; only the iOS warm run was repeated after the final decoder changes. No physical Android/iOS device was available, and remote SwiftPM publication was not performed because no hosting URL or publishing credentials were supplied.

## Retained pre-release artifact evidence

The following hashes are retained from the earlier closure evidence and are not attributed to `v0.0.13`; their manifests identify source commit `8c362e10fba65cbc2279f08c06eb5469e16a72e5`.

| Artifact | Path | SHA-256 / checksum |
|---|---|---|
| CLI | `Crossa/build/crossa` | `6dbd4978a41ec2ba1b11370ab6d97f7995f494d71e1b515fd656af5479e042b1` |
| Android Release AAR | `android-example/app/libs/crossa-generated-release.aar` | `297b6e05f5dd5cba091a7a2b7c3d14510064ba474d9a5bc18c74027642cf716c` |
| iOS Release XCFramework ZIP | `Crossa/build/crossa-ios-v0/release/Crossa.xcframework.zip` | `4651422f992c5250bc7612b18709728be3905f555845bca6cb97acaa8d53526a` |

The Android and iOS artifact manifests record that earlier source commit, CLI digest, runtime ABI, and Release configuration. The Android warm/cold raw result files were refreshed against that AAR; iOS simulator smoke results were validated against that XCFramework. These retained artifacts and measurements are historical evidence, not final-release artifact attribution.

## Hardening completed

- Typed scalar/model/list responses now use a bounded direct schema-aware decoder; only explicit `Json` uses the generic JSON representation.
- Direct decoding skips unknown fields, rejects duplicates and malformed/type-mismatched values, and enforces byte, cancellation, and nesting limits.
- ASAN and UBSAN are independent mutually exclusive CMake configurations with dedicated CI jobs and presets.
- Android Release generation has an explicit Gradle task dependency so the consumer cannot race artifact generation.
- iOS example deployment targets and SwiftUI result rendering now match the APIs used by the example and compile in Release.
- Source documentation and the generated website catalog describe V0 capabilities and post-V0 streaming scope consistently.

## Closure boundary

V0 closure covers the released source and validated gates above. V1 work, streaming delivery, physical-device validation, and remote distribution remain outside this closure.
