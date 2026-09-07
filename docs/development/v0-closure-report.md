# Crossa V0 Closure Report

Date: 2026-09-07

The closure validation used the current CLI build from Crossa commit
`3e37fbbc774edf1bd69f457f489ceb2f72d7cfe7` plus the working-tree closure
changes. The CLI reports version `0.1.0`.

## Results

| Area | Result | Evidence |
|---|---|---|
| CLI Release build | Passed | `build/v0-closure-cli/crossa`; `--version` = `0.1.0` |
| Native tests | Passed | `./test.sh`; language/runtime and CLI fixtures completed |
| Kotlin generator | Passed | `bash scripts/run-kotlin-generator-tests.sh ./build/v0-closure-cli/crossa` |
| Android project generator | Passed | `bash scripts/run-android-project-generator-tests.sh ./build/v0-closure-cli/crossa` |
| Android Release AAR | Passed | Fresh AAR built by generated Gradle project |
| Android app Debug/Release | Passed | `:app:assembleDebug :app:assembleRelease` |
| Android emulator warm benchmark | Passed | `Pixel_10_Pro`, Android 17, `arm64-v8a`, 8/8 per implementation |
| Android emulator cold benchmark | Passed | Fresh clients per measured sample, 8/8 per implementation |
| iOS Debug/Release XCFramework generation | Passed | Fresh CLI generation; device and simulator slices present |
| iOS simulator app Release build | Passed | Xcode 26.2, iOS Simulator SDK, `ARCHS=arm64` |
| iOS simulator warm benchmark | Passed | iPhone 17 on iOS 26.2, 8/8 per implementation |
| iOS simulator cold benchmark | Passed | Fresh Crossa runtime and Alamofire Session per sample, 8/8 |
| Doctor | Passed | All required rows passed with real tool paths |
| AddressSanitizer build | Passed | `build/v0-asan` configured and linked successfully |
| CI gates | Updated | Native, Kotlin generator, Android generator, and ASan jobs |

## Android artifact

The checked-in example artifact is:

`android-example/app/libs/crossa-generated-release.aar`

SHA-256:

`8fee24fe6258eced68dd730daaafea8a48769c246648d789c2dea0778c56d5f0`

The AAR contains `jni/arm64-v8a/libcrossa_runtime.so`. Its defined dynamic
symbol count is 6; the intentional JNI entry point is present and runtime
internals are hidden with `--exclude-libs,ALL`.

## iOS artifact

The checked-in example framework is:

`ios-example/CrossaBinary/Crossa.xcframework`

It contains `ios-arm64` and `ios-arm64-simulator` slices. The generated Release
SwiftPM ZIP checksum is:

`f3f4ee584aec283ecf8a9a9f997f6687c3f0d981abc14255a83051f989e206b9`

The generated package manifest, release template, checksum, ZIP, dSYM, and
artifact manifest were verified in the fresh output directory
`/private/tmp/crossa-v0-ios-release/release`.

## Runtime evidence

Both example apps now expose deterministic automation markers:

`CROSSA_BENCHMARK_STARTED`

`CROSSA_BENCHMARK_COMPLETED`

They persist raw samples and metadata as `benchmark-result.json` in the app
data container. Metadata includes endpoint, mode, warmups, measured iterations,
device/OS/ABI, artifact identity, SHA-256/checksum, and Crossa source commit.

Observed Android warm results were successful for Crossa, Retrofit, and Ktor;
observed Android cold results were successful for all three. Observed iOS warm
and cold results were successful for Crossa and Alamofire. These are simulator
and remote-network observations, not production performance claims.

## Remaining release boundary

No physical Android or iOS device was available for this run, so physical-device
performance and thermal evidence remain unverified. Remote SwiftPM publication
was not performed because no artifact hosting URL or publishing credentials were
provided; the local ZIP, checksum, release manifest template, and verification
path are ready for the repository that owns the generated SDK.
