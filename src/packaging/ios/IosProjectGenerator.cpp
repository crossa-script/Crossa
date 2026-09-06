#include "crossa/packaging/ios/IosProjectGenerator.h"

#include <algorithm>
#include <fstream>
#include <iterator>
#include <optional>
#include <set>
#include <stdexcept>
#include <system_error>

#include "crossa/compiler/generators/native/NativeProgramGenerator.h"
#include "crossa/packaging/ios/IosBuildRequirements.h"

using namespace std;

namespace crossa::packaging::ios {

    // Writes and copies deterministic files used by the generated iOS framework project.
    // generate() coordinates source embedding while writeProjectFiles() owns Xcode inputs.
    class IosProjectWriter final {
    public:
        // Writes the complete project after validating all supplied generated sources.
        static void generate(
            const vector<compiler::generators::swift::SwiftGeneratedSource>& swiftSources,
            const vector<const compiler::ir::Program*>& programs,
            const filesystem::path& outputDirectory
        ) {
            if (swiftSources.empty() || programs.empty()) {
                throw runtime_error("iOS generation requires linked Swift and native IR outputs.");
            }
            createDirectory(outputDirectory);
            const filesystem::path sources = outputDirectory / "Sources";
            writeNativeProgram(sources / "Native", programs);
            writeSwiftRuntime(sources / "Swift");
            for (const auto& source : swiftSources) {
                writeFile(sources / "Swift" / source.getFileName(), source.getContent());
            }
            writeBridgeHeaders(sources / "Bridge");
            writeProjectFiles(outputDirectory);
            writeBuildScripts(outputDirectory);
            writePackageWrapper(outputDirectory);
            writeManifestTemplate(outputDirectory, programs);
        }

        // Writes SwiftPM binary-package files for one archived XCFramework artifact.
        static void writeBinaryPackage(
            const filesystem::path& artifactRoot,
            const string& packageVersion,
            const optional<string>& packageBaseUrl
        ) {
            const filesystem::path checksumPath = artifactRoot / "checksum.txt";
            ifstream checksumInput(checksumPath);
            string checksum;
            checksumInput >> checksum;
            if (!checksumInput || checksum.empty()) {
                throw runtime_error(
                    "Unable to read SwiftPM checksum: " + checksumPath.string()
                );
            }

            const filesystem::path zipPath = artifactRoot / "Crossa.xcframework.zip";
            if (!filesystem::is_regular_file(zipPath)) {
                throw runtime_error("Missing XCFramework ZIP: " + zipPath.string());
            }

            string target;
            if (packageBaseUrl.has_value()) {
                target =
                    "            url: \"" + *packageBaseUrl + "/v" + packageVersion +
                    "/Crossa.xcframework.zip\",\n"
                    "            checksum: \"" + checksum + "\"\n";
            } else {
                target = "            path: \"Crossa.xcframework\"\n";
            }

            const string manifest =
                "// swift-tools-version: 6.0\n"
                "import PackageDescription\n\n"
                "// This binary package is generated from a project-specific Crossa SDK.\n"
                "let package = Package(\n"
                "    name: \"Crossa\",\n"
                "    platforms: [\n"
                "        .iOS(\"" + IosBuildRequirements::minimumDeploymentTarget() + "\")\n"
                "    ],\n"
                "    products: [\n"
                "        .library(name: \"Crossa\", targets: [\"Crossa\"])\n"
                "    ],\n"
                "    targets: [\n"
                "        .binaryTarget(\n"
                "            name: \"Crossa\",\n" +
                target +
                "        )\n"
                "    ]\n"
                ")\n";

            writeFile(artifactRoot / "Package.swift", manifest);
            writeFile(
                artifactRoot / "Package.swift.release.template",
                "// swift-tools-version: 6.0\n"
                "import PackageDescription\n\n"
                "let package = Package(\n"
                "    name: \"Crossa\",\n"
                "    platforms: [\n"
                "        .iOS(\"" + IosBuildRequirements::minimumDeploymentTarget() + "\")\n"
                "    ],\n"
                "    products: [\n"
                "        .library(name: \"Crossa\", targets: [\"Crossa\"])\n"
                "    ],\n"
                "    targets: [\n"
                "        .binaryTarget(\n"
                "            name: \"Crossa\",\n"
                "            url: \"<CROSSA_PACKAGE_BASE_URL>/v" + packageVersion +
                "/Crossa.xcframework.zip\",\n"
                "            checksum: \"" + checksum + "\"\n"
                "        )\n"
                "    ]\n"
                ")\n"
            );
            writeFile(
                artifactRoot / "artifact-manifest.package.json",
                "{\n"
                "  \"module\": \"Crossa\",\n"
                "  \"packageVersion\": \"" + packageVersion + "\",\n"
                "  \"checksum\": \"" + checksum + "\",\n"
                "  \"zip\": \"Crossa.xcframework.zip\",\n"
                "  \"ownership\": \"project-specific-generated-sdk\"\n"
                "}\n"
            );
        }

    private:
        // Creates one required directory or reports a deterministic generation error.
        static void createDirectory(const filesystem::path& directory) {
            error_code error;
            filesystem::create_directories(directory, error);
            if (error || !filesystem::is_directory(directory, error)) {
                throw runtime_error("Unable to create iOS output directory: " +
                    directory.string());
            }
        }

        // Writes one file atomically while preserving deterministic unchanged outputs.
        static void writeFile(const filesystem::path& path, const string& content) {
            createDirectory(path.parent_path());
            error_code error;
            if (filesystem::exists(path, error) && !error) {
                ifstream input(path, std::ios::binary);
                const string existing((istreambuf_iterator<char>(input)),
                    istreambuf_iterator<char>());
                if (input && existing == content) return;
            }
            filesystem::path temporary = path;
            temporary += ".tmp";
            ofstream output(temporary, std::ios::binary | std::ios::trunc);
            if (!output.is_open()) {
                throw runtime_error("Unable to write iOS generated file: " +
                    temporary.string());
            }
            output.write(content.data(), static_cast<streamsize>(content.size()));
            output.close();
            if (!output) {
                filesystem::remove(temporary, error);
                throw runtime_error("Unable to finish iOS generated file: " +
                    temporary.string());
            }
            filesystem::rename(temporary, path, error);
            if (error) {
                filesystem::remove(path, error);
                error.clear();
                filesystem::rename(temporary, path, error);
            }
            if (error) {
                filesystem::remove(temporary, error);
                throw runtime_error("Unable to finalize iOS generated file: " +
                    path.string());
            }
        }

        // Copies one native source tree needed by a standalone framework build.
        static void copyDirectory(
            const filesystem::path& source,
            const filesystem::path& destination
        ) {
            error_code error;
            filesystem::create_directories(destination.parent_path(), error);
            filesystem::copy(
                source,
                destination,
                filesystem::copy_options::recursive |
                    filesystem::copy_options::overwrite_existing,
                error
            );
            if (error) {
                throw runtime_error("Unable to embed iOS native source: " +
                    source.string());
            }
        }

        // Copies one focused native source file needed by generated program reconstruction.
        static void copyFile(
            const filesystem::path& source,
            const filesystem::path& destination
        ) {
            createDirectory(destination.parent_path());
            error_code error;
            filesystem::copy_file(
                source,
                destination,
                filesystem::copy_options::overwrite_existing,
                error
            );
            if (error) {
                throw runtime_error("Unable to embed iOS native file: " +
                    source.string());
            }
        }

        // Embeds native runtime sources and compiler-specialized generated program code.
        static void writeNativeProgram(
            const filesystem::path& nativeDirectory,
            const vector<const compiler::ir::Program*>& programs
        ) {
#ifdef CROSSA_SOURCE_DIRECTORY
            const filesystem::path sourceRoot(CROSSA_SOURCE_DIRECTORY);
            copyDirectory(sourceRoot / "include" / "crossa",
                nativeDirectory / "Runtime" / "include" / "crossa");
            const vector<filesystem::path> directories = {
                "src/runtime", "src/network", "src/bindings/shared-abi",
                "src/bindings/ios", "src/compiler/ir", "src/utils"
            };
            for (const filesystem::path& directory : directories) {
                copyDirectory(sourceRoot / directory,
                    nativeDirectory / "Runtime" / directory);
            }
            copyFile(sourceRoot / "src/compiler/source/SourceLocation.cpp",
                nativeDirectory / "Runtime" / "src/compiler/source/SourceLocation.cpp");
            copyFile(sourceRoot / "src/compiler/types/SemanticType.cpp",
                nativeDirectory / "Runtime" / "src/compiler/types/SemanticType.cpp");
#else
            throw runtime_error("iOS generation requires the Crossa runtime source directory.");
#endif
            compiler::generators::native::NativeProgramGenerator generator;
            writeFile(nativeDirectory / "CrossaGeneratedProgram.h",
                generator.generateProgramHeader());
            writeFile(nativeDirectory / "CrossaGeneratedProgram.cpp",
                generator.generateProgramSource(programs));
            writeFile(nativeDirectory / "CrossaGeneratedOperations.h",
                generator.generateOperationHeader(programs));
        }

        // Writes the Swift runtime that retains native result ownership and maps ABI states.
        static void writeSwiftRuntime(const filesystem::path& directory) {
            writeFile(directory / "CrossaState.swift",
                "import Foundation\n\n"
                "public enum CrossaState<Value>: @unchecked Sendable {\n"
                "    case success(Value)\n"
                "    case failed(CrossaError)\n"
                "    case cancelled\n"
                "}\n");
            writeFile(directory / "CrossaError.swift",
                "import Foundation\n\n"
                "public struct CrossaError: Error, Sendable {\n"
                "    public let message: String\n"
                "    public let domain: Int32\n"
                "    public let code: Int32\n"
                "    public let retryable: Bool\n\n"
                "    internal init(message: String, domain: Int32, code: Int32, retryable: Bool) {\n"
                "        self.message = message\n"
                "        self.domain = domain\n"
                "        self.code = code\n"
                "        self.retryable = retryable\n"
                "    }\n"
                "}\n");
            writeFile(directory / "CrossaNativeValue.swift", nativeValueSource());
            writeFile(directory / "CrossaRuntime.swift", runtimeSource());
        }

        // Returns the private Swift ABI adapter and lazy model/list result implementation.
        [[nodiscard]] static string nativeValueSource() {
            return R"SWIFT(import Foundation

internal enum CrossaArgument {
    case int(Int)
    case long(Int64)
    case double(Double)
    case bool(Bool)
    case string(String)

    internal init(_ value: Int) { self = .int(value) }
    internal init(_ value: Int64) { self = .long(value) }
    internal init(_ value: Double) { self = .double(value) }
    internal init(_ value: Bool) { self = .bool(value) }
    internal init(_ value: String) { self = .string(value) }
}

internal final class CrossaNativeResult: @unchecked Sendable {
    private let runtime: UInt64
    private let result: UInt64
    private let lock = NSLock()
    private var released = false

    internal init(runtime: UInt64, result: UInt64) {
        self.runtime = runtime
        self.result = result
    }

    deinit { release() }

    internal func rootValue() -> CrossaNativeValue {
        CrossaNativeValue(owner: self, path: [])
    }

    internal func release() {
        lock.lock()
        defer { lock.unlock() }
        guard !released else { return }
        released = true
        _ = crossaReleaseResult(runtime, result)
    }

    internal func takeInt() -> Int { defer { release() }; return rootValue().int() }
    internal func takeLong() -> Int64 { defer { release() }; return rootValue().long() }
    internal func takeDouble() -> Double { defer { release() }; return rootValue().double() }
    internal func takeString() -> String { defer { release() }; return rootValue().string() }
    internal func takeBool() -> Bool { defer { release() }; return rootValue().bool() }

    fileprivate func withPath<Result>(_ path: [CrossaAbiPathSegment], _ body: (UnsafePointer<CrossaAbiPathSegment>?, Int) -> Result) -> Result {
        path.withUnsafeBufferPointer { buffer in body(buffer.baseAddress, buffer.count) }
    }

    fileprivate func require(_ status: CrossaStatus) {
        if status != CrossaStatusOk {
            preconditionFailure("Crossa native ABI access failed with status \(status.rawValue).")
        }
    }

    fileprivate func int(at path: [CrossaAbiPathSegment]) -> Int {
        var value: Int32 = 0
        withPath(path) { pointer, count in require(crossaGetPathInt(runtime, result, pointer, count, &value)) }
        return Int(value)
    }

    fileprivate func long(at path: [CrossaAbiPathSegment]) -> Int64 {
        var value: Int64 = 0
        withPath(path) { pointer, count in require(crossaGetPathLong(runtime, result, pointer, count, &value)) }
        return value
    }

    fileprivate func double(at path: [CrossaAbiPathSegment]) -> Double {
        var value: Double = 0
        withPath(path) { pointer, count in require(crossaGetPathDouble(runtime, result, pointer, count, &value)) }
        return value
    }

    fileprivate func bool(at path: [CrossaAbiPathSegment]) -> Bool {
        var value: UInt8 = 0
        withPath(path) { pointer, count in require(crossaGetPathBool(runtime, result, pointer, count, &value)) }
        return value != 0
    }

    fileprivate func string(at path: [CrossaAbiPathSegment]) -> String {
        var value = CrossaStringView(data: nil, size: 0)
        withPath(path) { pointer, count in require(crossaGetPathString(runtime, result, pointer, count, &value)) }
        guard let data = value.data else { return "" }
        return String(decoding: UnsafeBufferPointer(start: UnsafeRawPointer(data).assumingMemoryBound(to: UInt8.self), count: value.size), as: UTF8.self)
    }

    fileprivate func listSize(at path: [CrossaAbiPathSegment]) -> Int {
        var value = 0
        withPath(path) { pointer, count in require(crossaGetPathListSize(runtime, result, pointer, count, &value)) }
        return value
    }
}

internal struct CrossaNativeValue: @unchecked Sendable {
    fileprivate let owner: CrossaNativeResult
    fileprivate let path: [CrossaAbiPathSegment]

    internal func child(field: UInt32) -> CrossaNativeValue {
        CrossaNativeValue(owner: owner, path: path + [CrossaAbiPathSegment(kind: CrossaAbiPathField, index: field)])
    }

    internal func element(index: Int) -> CrossaNativeValue {
        CrossaNativeValue(owner: owner, path: path + [CrossaAbiPathSegment(kind: CrossaAbiPathListElement, index: UInt32(index))])
    }

    internal func int() -> Int { owner.int(at: path) }
    internal func long() -> Int64 { owner.long(at: path) }
    internal func double() -> Double { owner.double(at: path) }
    internal func string() -> String { owner.string(at: path) }
    internal func bool() -> Bool { owner.bool(at: path) }
    internal func listSize() -> Int { owner.listSize(at: path) }
}

public struct CrossaList<Element>: RandomAccessCollection, @unchecked Sendable {
    public typealias Index = Int

    private let nativeValue: CrossaNativeValue
    private let map: (CrossaNativeValue) -> Element

    internal init(nativeValue: CrossaNativeValue, map: @escaping (CrossaNativeValue) -> Element) {
        self.nativeValue = nativeValue
        self.map = map
    }

    public var startIndex: Int { 0 }
    public var endIndex: Int { nativeValue.listSize() }

    public func index(after index: Int) -> Int { index + 1 }
    public func index(before index: Int) -> Int { index - 1 }

    public subscript(position: Int) -> Element {
        precondition(position >= startIndex && position < endIndex, "Crossa list index is out of bounds.")
        return map(nativeValue.element(index: position))
    }
}
)SWIFT";
        }

        // Returns the Swift runtime lifecycle and exactly-once callback bridge implementation.
        [[nodiscard]] static string runtimeSource() {
            return R"SWIFT(import Foundation

public final class CrossaOperation: @unchecked Sendable {
    private let runtime: UInt64
    private let handle: UInt64
    private let lock = NSLock()
    private var released = false

    internal init(runtime: UInt64, handle: UInt64) {
        self.runtime = runtime
        self.handle = handle
    }

    deinit { release() }

    public func cancel() {
        lock.lock()
        defer { lock.unlock() }
        guard !released else { return }
        _ = crossaCancelOperation(runtime, handle)
    }

    public func release() {
        lock.lock()
        defer { lock.unlock() }
        guard !released else { return }
        released = true
        _ = crossaReleaseOperation(runtime, handle)
    }
}

private class CrossaCompletionOwner {
    fileprivate func complete(kind: CrossaAbiCompletionKind, result: UInt64, error: UInt64) {}
}

private final class CrossaTypedCompletionOwner<Value>: CrossaCompletionOwner {
    private let runtime: UInt64
    private let map: (CrossaNativeResult) -> Value
    private let completion: (CrossaState<Value>) -> Void

    init(runtime: UInt64, map: @escaping (CrossaNativeResult) -> Value, completion: @escaping (CrossaState<Value>) -> Void) {
        self.runtime = runtime
        self.map = map
        self.completion = completion
    }

    override func complete(kind: CrossaAbiCompletionKind, result: UInt64, error: UInt64) {
        switch kind {
        case CrossaAbiCompletionSuccess:
            completion(.success(map(CrossaNativeResult(runtime: runtime, result: result))))
        case CrossaAbiCompletionFailed:
            completion(.failed(CrossaRuntime.readError(runtime: runtime, error: error)))
        case CrossaAbiCompletionCancelled:
            completion(.cancelled)
        default:
            completion(.failed(CrossaError(message: "Crossa delivered an unknown terminal state.", domain: 0, code: 0, retryable: false)))
        }
    }
}

private final class CrossaAsyncBridge<Value>: @unchecked Sendable {
    private let lock = NSLock()
    private var continuation: CheckedContinuation<Value, Error>?
    private var operation: CrossaOperation?
    private var completed = false
    private var cancelled = false

    internal func begin(_ continuation: CheckedContinuation<Value, Error>) -> Bool {
        lock.lock()
        let shouldCancel = cancelled
        if !shouldCancel {
            self.continuation = continuation
        }
        lock.unlock()
        if shouldCancel {
            continuation.resume(throwing: CancellationError())
            return false
        }
        return true
    }

    internal func attach(_ operation: CrossaOperation) {
        lock.lock()
        let shouldCancel = cancelled || completed
        if !shouldCancel {
            self.operation = operation
        }
        lock.unlock()
        if shouldCancel {
            operation.cancel()
        }
    }

    internal func cancel() {
        lock.lock()
        cancelled = true
        let operation = self.operation
        self.operation = nil
        let continuation = completed ? nil : self.continuation
        self.continuation = nil
        completed = true
        lock.unlock()
        operation?.cancel()
        continuation?.resume(throwing: CancellationError())
    }

    internal func complete(_ state: CrossaState<Value>) {
        lock.lock()
        guard !completed else {
            lock.unlock()
            return
        }
        completed = true
        let continuation = self.continuation
        self.continuation = nil
        self.operation = nil
        lock.unlock()
        guard let continuation else { return }
        switch state {
        case .success(let value):
            continuation.resume(returning: value)
        case .failed(let error):
            continuation.resume(throwing: error)
        case .cancelled:
            continuation.resume(throwing: CancellationError())
        }
    }
}

private func crossaCompletion(_ userData: UnsafeMutableRawPointer?, _ kind: CrossaAbiCompletionKind, _ result: UInt64, _ error: UInt64) {
    guard let userData else { return }
    Unmanaged<CrossaCompletionOwner>.fromOpaque(userData).takeRetainedValue().complete(kind: kind, result: result, error: error)
}

public final class CrossaRuntime: @unchecked Sendable {
    private let lock = NSLock()
    private var runtime: UInt64

    public init() throws {
        var handle: UInt64 = 0
        let status = crossaIosCreateGeneratedRuntime(&handle)
        guard status == CrossaStatusOk && handle != 0 else {
            throw CrossaError(message: "Crossa native runtime creation failed.", domain: 0, code: Int32(status.rawValue), retryable: false)
        }
        runtime = handle
    }

    deinit { close() }

    public func shutdown() {
        lock.lock()
        defer { lock.unlock() }
        guard runtime != 0 else { return }
        _ = crossaRuntimeShutdown(runtime)
    }

    public func close() {
        lock.lock()
        defer { lock.unlock() }
        guard runtime != 0 else { return }
        let current = runtime
        runtime = 0
        crossaReleaseRuntime(current)
    }

    internal func invokeAsync(operation: UInt64, arguments: [CrossaArgument]) -> CrossaOperation {
        let handle = requireHandle()
        return withArguments(arguments) { values, count in
            var operationHandle: UInt64 = 0
            let status = crossaInvokeAsync(handle, operation, values, count, &operationHandle)
            precondition(status == CrossaStatusOk && operationHandle != 0, "Crossa native async invocation failed.")
            return CrossaOperation(runtime: handle, handle: operationHandle)
        }
    }

    internal func invokeAsyncAfter<Value>(operation: UInt64, arguments: [CrossaArgument], map: @escaping (CrossaNativeResult) -> Value, onState: @escaping (CrossaState<Value>) -> Void) -> CrossaOperation {
        let handle = requireHandle()
        let owner = CrossaTypedCompletionOwner(runtime: handle, map: map, completion: onState)
        let retained = Unmanaged.passRetained(owner).toOpaque()
        return withArguments(arguments) { values, count in
            var operationHandle: UInt64 = 0
            let status = crossaInvokeAsyncAfter(handle, operation, values, count, crossaCompletion, retained, &operationHandle)
            guard status == CrossaStatusOk && operationHandle != 0 else {
                Unmanaged<CrossaCompletionOwner>.fromOpaque(retained).release()
                preconditionFailure("Crossa native async completion invocation failed.")
            }
            return CrossaOperation(runtime: handle, handle: operationHandle)
        }
    }

    internal func invokeAsyncAfterAwait<Value>(operation: UInt64, arguments: [CrossaArgument], map: @escaping (CrossaNativeResult) -> Value) async throws -> Value {
        let bridge = CrossaAsyncBridge<Value>()
        return try await withTaskCancellationHandler(operation: {
            try await withCheckedThrowingContinuation { (continuation: CheckedContinuation<Value, Error>) in
                guard bridge.begin(continuation) else { return }
                let nativeOperation = invokeAsyncAfter(operation: operation, arguments: arguments, map: map) { state in
                    bridge.complete(state)
                }
                bridge.attach(nativeOperation)
            }
        }, onCancel: {
            bridge.cancel()
        })
    }

    fileprivate static func readError(runtime: UInt64, error: UInt64) -> CrossaError {
        defer { _ = crossaReleaseError(runtime, error) }
        var message = CrossaStringView(data: nil, size: 0)
        var domain: Int32 = 0
        var code: Int32 = 0
        var retryable: UInt8 = 0
        let messageStatus = crossaGetErrorMessage(runtime, error, &message)
        let metadataStatus = crossaGetErrorMetadata(runtime, error, &domain, &code, &retryable)
        guard messageStatus == CrossaStatusOk, metadataStatus == CrossaStatusOk, let data = message.data else {
            return CrossaError(message: "Crossa native operation failed.", domain: domain, code: code, retryable: retryable != 0)
        }
        return CrossaError(message: String(decoding: UnsafeBufferPointer(start: UnsafeRawPointer(data).assumingMemoryBound(to: UInt8.self), count: message.size), as: UTF8.self), domain: domain, code: code, retryable: retryable != 0)
    }

    private func requireHandle() -> UInt64 {
        lock.lock()
        defer { lock.unlock() }
        precondition(runtime != 0, "Crossa runtime is closed.")
        return runtime
    }

    private func withArguments<Result>(_ arguments: [CrossaArgument], _ body: (UnsafePointer<CrossaAbiArgument>?, Int) -> Result) -> Result {
        let strings = arguments.map { argument -> Data in
            if case let .string(value) = argument { return Data(value.utf8) }
            return Data()
        }
        var values = arguments.enumerated().map { index, argument -> CrossaAbiArgument in
            switch argument {
            case let .int(value): return CrossaAbiArgument(kind: CrossaAbiArgumentInt, integerValue: Int64(value), doubleValue: 0, stringValue: CrossaStringView(data: nil, size: 0))
            case let .long(value): return CrossaAbiArgument(kind: CrossaAbiArgumentLong, integerValue: value, doubleValue: 0, stringValue: CrossaStringView(data: nil, size: 0))
            case let .double(value): return CrossaAbiArgument(kind: CrossaAbiArgumentDouble, integerValue: 0, doubleValue: value, stringValue: CrossaStringView(data: nil, size: 0))
            case let .bool(value): return CrossaAbiArgument(kind: CrossaAbiArgumentBool, integerValue: value ? 1 : 0, doubleValue: 0, stringValue: CrossaStringView(data: nil, size: 0))
            case .string: return CrossaAbiArgument(kind: CrossaAbiArgumentString, integerValue: 0, doubleValue: 0, stringValue: CrossaStringView(data: nil, size: strings[index].count))
            }
        }
        return strings.withUnsafeBufferPointer { stringBuffer in
            for index in values.indices where values[index].kind == CrossaAbiArgumentString {
                values[index].stringValue.data = stringBuffer[index].withUnsafeBytes { $0.bindMemory(to: CChar.self).baseAddress }
            }
            return values.withUnsafeBufferPointer { buffer in body(buffer.baseAddress, buffer.count) }
        }
    }
}
)SWIFT";
        }

        // Writes private bridge and public umbrella headers for the Swift framework target.
        static void writeBridgeHeaders(const filesystem::path& directory) {
            writeFile(directory / "Crossa.h",
                "#import <Foundation/Foundation.h>\n"
                "#import \"CrossaBridge.h\"\n\n"
                "FOUNDATION_EXPORT double CrossaVersionNumber;\n"
                "FOUNDATION_EXPORT const unsigned char CrossaVersionString[];\n");
            writeFile(directory / "CrossaBridge.h",
                "#include <crossa/bindings/ios/CrossaIosRuntimeBridge.h>\n"
                "#include <crossa/bindings/shared-abi/CrossaAbi.h>\n");
        }

        // Writes Xcode settings, source synchronization project data, and distribution options.
        static void writeProjectFiles(const filesystem::path& directory) {
            writeFile(directory / "Config" / "Crossa.xcconfig",
                "IPHONEOS_DEPLOYMENT_TARGET = " +
                IosBuildRequirements::minimumDeploymentTarget() + "\n"
                "SDKROOT = iphoneos\n"
                "SUPPORTED_PLATFORMS = iphoneos iphonesimulator\n"
                "SWIFT_VERSION = 6.0\n"
                "BUILD_LIBRARY_FOR_DISTRIBUTION = YES\n"
                "SKIP_INSTALL = NO\n"
                "DEFINES_MODULE = YES\n"
                "LD_DYLIB_INSTALL_NAME = @rpath/$(PRODUCT_NAME).framework/$(PRODUCT_NAME)\n"
                "GCC_C_LANGUAGE_STANDARD = gnu17\n"
                "CLANG_CXX_LANGUAGE_STANDARD = c++20\n"
                "GCC_SYMBOLS_PRIVATE_EXTERN = YES\n"
                "OTHER_CPLUSPLUSFLAGS = $(inherited) -fvisibility=hidden -fvisibility-inlines-hidden -ffile-prefix-map=$(PROJECT_DIR)=/crossa-source\n"
                "HEADER_SEARCH_PATHS = $(PROJECT_DIR)/Sources/Native/Runtime/include $(PROJECT_DIR)/Sources/Native $(PROJECT_DIR)/.crossa/dependencies/$(PLATFORM_NAME)/$(ARCHS)/curl/include\n"
                "OTHER_LDFLAGS = $(inherited) $(PROJECT_DIR)/.crossa/dependencies/$(PLATFORM_NAME)/$(ARCHS)/curl/lib/libcurl.a -framework Security -framework SystemConfiguration -framework CFNetwork -lz\n"
            );
            writeFile(directory / "Crossa.xcodeproj" / "project.pbxproj", projectFile());
        }

        // Returns the minimal Xcode project using a synchronized source group for generated files.
        [[nodiscard]] static string projectFile() {
            return R"PBX(// !$*UTF8*$!
{
    archiveVersion = 1;
    classes = {};
    objectVersion = 77;
    objects = {
        000000000000000000000001 = {isa = PBXBuildFile; fileRef = 000000000000000000000002; settings = {ATTRIBUTES = (Public, ); }; };
        000000000000000000000002 = {isa = PBXFileReference; lastKnownFileType = sourcecode.c.h; path = Sources/Bridge/Crossa.h; sourceTree = SOURCE_ROOT; };
        000000000000000000000003 = {isa = PBXFileSystemSynchronizedRootGroup; exceptions = (000000000000000000000004, ); path = Sources; sourceTree = "<group>"; };
        000000000000000000000004 = {isa = PBXFileSystemSynchronizedBuildFileExceptionSet; membershipExceptions = (Bridge/Crossa.h, ); target = 000000000000000000000005; };
        000000000000000000000006 = {isa = PBXFrameworksBuildPhase; buildActionMask = 2147483647; files = (); runOnlyForDeploymentPostprocessing = 0; };
        000000000000000000000007 = {isa = PBXHeadersBuildPhase; buildActionMask = 2147483647; files = (000000000000000000000001, ); runOnlyForDeploymentPostprocessing = 0; };
        000000000000000000000008 = {isa = PBXSourcesBuildPhase; buildActionMask = 2147483647; files = (); runOnlyForDeploymentPostprocessing = 0; };
        000000000000000000000009 = {isa = PBXShellScriptBuildPhase; buildActionMask = 2147483647; files = (); inputPaths = (); name = "Prepare Crossa dependencies"; outputPaths = (); runOnlyForDeploymentPostprocessing = 0; shellPath = /bin/sh; shellScript = "\"${PROJECT_DIR}/Scripts/prepare-dependencies.sh\"\n"; };
        000000000000000000000005 = {isa = PBXNativeTarget; buildConfigurationList = 000000000000000000000010; buildPhases = (000000000000000000000009, 000000000000000000000008, 000000000000000000000007, 000000000000000000000006, ); buildRules = (); dependencies = (); fileSystemSynchronizedGroups = (000000000000000000000003, ); name = Crossa; productName = Crossa; productReference = 000000000000000000000011; productType = "com.apple.product-type.framework"; };
        000000000000000000000011 = {isa = PBXFileReference; explicitFileType = wrapper.framework; includeInIndex = 0; path = Crossa.framework; sourceTree = BUILT_PRODUCTS_DIR; };
        000000000000000000000012 = {isa = PBXGroup; children = (000000000000000000000003, 000000000000000000000002, 000000000000000000000013, ); sourceTree = "<group>"; };
        000000000000000000000013 = {isa = PBXGroup; children = (000000000000000000000011, ); name = Products; sourceTree = "<group>"; };
        000000000000000000000014 = {isa = PBXProject; attributes = {BuildIndependentTargetsInParallel = 1; LastSwiftUpdateCheck = 2620; LastUpgradeCheck = 2620; TargetAttributes = {000000000000000000000005 = {CreatedOnToolsVersion = 26.2; }; }; }; buildConfigurationList = 000000000000000000000015; compatibilityVersion = "Xcode 16.0"; developmentRegion = en; hasScannedForEncodings = 0; knownRegions = (en, Base, ); mainGroup = 000000000000000000000012; productRefGroup = 000000000000000000000013; projectDirPath = ""; projectRoot = ""; targets = (000000000000000000000005, ); };
        000000000000000000000010 = {isa = XCConfigurationList; buildConfigurations = (000000000000000000000016, 000000000000000000000017, ); defaultConfigurationIsVisible = 0; defaultConfigurationName = Release; };
        000000000000000000000015 = {isa = XCConfigurationList; buildConfigurations = (000000000000000000000018, 000000000000000000000019, ); defaultConfigurationIsVisible = 0; defaultConfigurationName = Release; };
        000000000000000000000016 = {isa = XCBuildConfiguration; baseConfigurationReference = 000000000000000000000020; buildSettings = {GENERATE_INFOPLIST_FILE = YES; PRODUCT_BUNDLE_IDENTIFIER = io.crossa.framework; PRODUCT_MODULE_NAME = Crossa; PRODUCT_NAME = Crossa; SDKROOT = iphoneos; SUPPORTED_PLATFORMS = (iphoneos, iphonesimulator, ); SWIFT_OPTIMIZATION_LEVEL = "-Onone"; }; name = Debug; };
        000000000000000000000017 = {isa = XCBuildConfiguration; baseConfigurationReference = 000000000000000000000020; buildSettings = {GENERATE_INFOPLIST_FILE = YES; PRODUCT_BUNDLE_IDENTIFIER = io.crossa.framework; PRODUCT_MODULE_NAME = Crossa; PRODUCT_NAME = Crossa; SDKROOT = iphoneos; SUPPORTED_PLATFORMS = (iphoneos, iphonesimulator, ); SWIFT_COMPILATION_MODE = wholemodule; SWIFT_OPTIMIZATION_LEVEL = "-O"; DEAD_CODE_STRIPPING = YES; }; name = Release; };
        000000000000000000000018 = {isa = XCBuildConfiguration; baseConfigurationReference = 000000000000000000000020; buildSettings = {}; name = Debug; };
        000000000000000000000019 = {isa = XCBuildConfiguration; baseConfigurationReference = 000000000000000000000020; buildSettings = {}; name = Release; };
        000000000000000000000020 = {isa = PBXFileReference; lastKnownFileType = text.xcconfig; path = Config/Crossa.xcconfig; sourceTree = SOURCE_ROOT; };
    };
    rootObject = 000000000000000000000014;
}
)PBX";
        }

        // Writes focused dependency and archive scripts for deterministic XCFramework creation.
        static void writeBuildScripts(const filesystem::path& directory) {
            writeFile(directory / "Scripts" / "prepare-dependencies.sh", dependencyScript());
            writeFile(directory / "Scripts" / "build-xcframework.sh", archiveScript());
            writeFile(directory / "Dependencies" / "CMakeLists.txt", dependencyCmake());
            setExecutable(directory / "Scripts" / "prepare-dependencies.sh");
            setExecutable(directory / "Scripts" / "build-xcframework.sh");
        }

        // Marks one generated script executable without changing unrelated filesystem entries.
        static void setExecutable(const filesystem::path& path) {
            error_code error;
            filesystem::permissions(path,
                filesystem::perms::owner_exec | filesystem::perms::group_exec |
                    filesystem::perms::others_exec,
                filesystem::perm_options::add,
                error);
            if (error) throw runtime_error("Unable to make iOS script executable: " + path.string());
        }

        // Returns the source-verified native curl provisioning script used by Xcode builds.
        [[nodiscard]] static string dependencyScript() {
            return "#!/bin/sh\nset -eu\n\n"
                "if ! command -v cmake >/dev/null 2>&1; then\n"
                "    echo 'Crossa iOS build requires CMake to provision libcurl.' >&2\n    exit 1\nfi\n"
                "if [ \"${PLATFORM_NAME:-}\" = \"iphonesimulator\" ]; then platform=simulator; else platform=device; fi\n"
                "arch=${CURRENT_ARCH:-}\n"
                "if [ -z \"${arch}\" ] || [ \"${arch}\" = undefined_arch ]; then arch=${ARCHS:-}; fi\n"
                "if [ -z \"${arch}\" ] || [ \"${arch}\" = undefined_arch ]; then\n"
                "    echo 'Crossa requires a concrete Xcode architecture.' >&2\n    exit 1\nfi\n"
                "build=\"${PROJECT_DIR}/.crossa/dependencies/${PLATFORM_NAME}/${arch}\"\n"
                "if [ -f \"${build}/curl/lib/libcurl.a\" ]; then exit 0; fi\n"
                "cmake_bin=$(command -v cmake)\n"
                "ninja_bin=$(dirname \"${cmake_bin}\")/ninja\n"
                "if [ ! -x \"${ninja_bin}\" ]; then ninja_bin=$(command -v ninja); fi\n"
                "if [ -z \"${ninja_bin}\" ] || [ ! -x \"${ninja_bin}\" ]; then\n"
                "    echo 'Crossa iOS build requires Ninja to provision libcurl.' >&2\n    exit 1\nfi\n"
                "clang=$(xcrun --sdk \"${PLATFORM_NAME}\" --find clang)\n"
                "clangxx=$(xcrun --sdk \"${PLATFORM_NAME}\" --find clang++)\n"
                "cmake -S \"${PROJECT_DIR}/Dependencies\" -B \"${build}/build\" -G Ninja "
                "-DCMAKE_MAKE_PROGRAM=\"${ninja_bin}\" "
                "-DCMAKE_C_COMPILER=\"${clang}\" -DCMAKE_CXX_COMPILER=\"${clangxx}\" "
                "-DCROSSA_PLATFORM=\"${platform}\" -DCROSSA_ARCH=\"${arch}\" "
                "-DCROSSA_SDKROOT=\"${SDKROOT}\" -DCROSSA_DEPLOYMENT_TARGET=\"${IPHONEOS_DEPLOYMENT_TARGET}\" "
                "-DCROSSA_INSTALL_ROOT=\"${build}\"\n"
                "cmake --build \"${build}/build\"\n";
        }

        // Returns the normal archive and XCFramework flow used by the CLI packaging command.
        [[nodiscard]] static string archiveScript() {
            return "#!/bin/sh\nset -eu\n\n"
                "project_dir=$(CDPATH= cd -- \"$(dirname -- \"$0\")/..\" && pwd)\n"
                "configuration=${1:?configuration is required}\n"
                "artifact_root=${2:?artifact root is required}\n"
                "xcode_configuration=Release\n"
                "if [ \"${configuration}\" = debug ]; then xcode_configuration=Debug; fi\n"
                "simulator_arch=arm64\n"
                "device_archive=\"${artifact_root}/archives/device.xcarchive\"\n"
                "simulator_archive=\"${artifact_root}/archives/simulator.xcarchive\"\n"
                "rm -rf \"${device_archive}\" \"${simulator_archive}\" \"${artifact_root}/Crossa.xcframework\"\n"
                "derived_data=\"${artifact_root}/derived-data\"\n"
                "xcodebuild archive -project \"${project_dir}/Crossa.xcodeproj\" -scheme Crossa -configuration \"${xcode_configuration}\" -destination 'generic/platform=iOS' ARCHS=arm64 -archivePath \"${device_archive}\" -derivedDataPath \"${derived_data}/device\"\n"
                "xcodebuild archive -project \"${project_dir}/Crossa.xcodeproj\" -scheme Crossa -configuration \"${xcode_configuration}\" -destination 'generic/platform=iOS Simulator' ARCHS=\"${simulator_arch}\" -archivePath \"${simulator_archive}\" -derivedDataPath \"${derived_data}/simulator\"\n"
                "for archive in \"${device_archive}\" \"${simulator_archive}\"; do\n"
                "    framework=\"${archive}/Products/Library/Frameworks/Crossa.framework\"\n"
                "    sed -i '' '/#import \"CrossaBridge.h\"/d' \"${framework}/Headers/Crossa.h\"\n"
                "    find \"${framework}/Modules\" -name '*.swiftinterface' -type f -exec sed -i '' '/^@_exported import Crossa$/d' {} +\n"
                "done\n"
                "xcodebuild -create-xcframework -archive \"${device_archive}\" -framework Crossa.framework -archive \"${simulator_archive}\" -framework Crossa.framework -output \"${artifact_root}/Crossa.xcframework\"\n"
                "mkdir -p \"${artifact_root}/symbols\" \"${artifact_root}/metadata\"\n"
                "find \"${device_archive}\" \"${simulator_archive}\" -name '*.dSYM' -type d -exec cp -R {} \"${artifact_root}/symbols/\" \\;\n"
                "mkdir -p \"${artifact_root}/package\"\n"
                "(cd \"${artifact_root}\" && zip -qryX \"package/Crossa.xcframework.zip\" Crossa.xcframework)\n"
                "cp \"${artifact_root}/package/Crossa.xcframework.zip\" \"${artifact_root}/Crossa.xcframework.zip\"\n"
                "swift package compute-checksum \"${artifact_root}/package/Crossa.xcframework.zip\" > \"${artifact_root}/package/Crossa.xcframework.checksum\"\n"
                "cp \"${artifact_root}/package/Crossa.xcframework.checksum\" \"${artifact_root}/checksum.txt\"\n";
        }

        // Returns isolated CMake dependency provisioning for the iOS C++ runtime.
        [[nodiscard]] static string dependencyCmake() {
            return "cmake_minimum_required(VERSION 3.22)\n"
                "foreach(required CROSSA_PLATFORM CROSSA_ARCH CROSSA_SDKROOT CROSSA_DEPLOYMENT_TARGET CROSSA_INSTALL_ROOT)\n"
                "    if(NOT DEFINED ${required})\n        message(FATAL_ERROR \"Missing ${required} for Crossa iOS dependencies.\")\n    endif()\n"
                "endforeach()\n\n"
                "set(CMAKE_SYSTEM_NAME iOS)\n"
                "set(CMAKE_OSX_SYSROOT \"${CROSSA_SDKROOT}\")\n"
                "set(CMAKE_OSX_ARCHITECTURES \"${CROSSA_ARCH}\")\n"
                "set(CMAKE_OSX_DEPLOYMENT_TARGET \"${CROSSA_DEPLOYMENT_TARGET}\")\n"
                "set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)\n"
                "set(CMAKE_XCODE_ATTRIBUTE_CODE_SIGNING_ALLOWED NO)\n"
                "set(CMAKE_XCODE_ATTRIBUTE_CODE_SIGNING_REQUIRED NO)\n"
                "set(CMAKE_XCODE_ATTRIBUTE_GENERATE_INFOPLIST_FILE YES)\n"
                "project(CrossaIosDependencies LANGUAGES C CXX)\n\n"
                "include(ExternalProject)\n"
                "set(CROSSA_CURL_INSTALL \"${CROSSA_INSTALL_ROOT}/curl\")\n"
                "ExternalProject_Add(crossa_ios_curl\n"
                "    URL \"" + IosBuildRequirements::curlArchiveUrl() + "\"\n"
                "    URL_HASH \"SHA256=" + IosBuildRequirements::curlArchiveSha256() + "\"\n"
                "    CMAKE_ARGS -DCMAKE_SYSTEM_NAME=iOS -DCMAKE_OSX_SYSROOT=${CROSSA_SDKROOT} -DCMAKE_OSX_ARCHITECTURES=${CROSSA_ARCH} -DCMAKE_OSX_DEPLOYMENT_TARGET=${CROSSA_DEPLOYMENT_TARGET} -DCMAKE_TRY_COMPILE_TARGET_TYPE=STATIC_LIBRARY -DCMAKE_XCODE_ATTRIBUTE_CODE_SIGNING_ALLOWED=NO -DCMAKE_XCODE_ATTRIBUTE_CODE_SIGNING_REQUIRED=NO -DCMAKE_XCODE_ATTRIBUTE_GENERATE_INFOPLIST_FILE=YES -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=${CROSSA_CURL_INSTALL} -DBUILD_SHARED_LIBS=OFF -DBUILD_CURL_EXE=OFF -DBUILD_TESTING=OFF -DCURL_USE_SECTRANSP=ON -DCURL_USE_OPENSSL=OFF -DCURL_ZLIB=ON -DCURL_USE_LIBPSL=OFF -DCURL_BROTLI=OFF -DCURL_ZSTD=OFF -DUSE_NGHTTP2=OFF -DCURL_USE_LIBSSH2=OFF -DCURL_DISABLE_LDAP=ON -DCURL_DISABLE_LDAPS=ON -DCURL_DISABLE_RTSP=ON -DCURL_DISABLE_DICT=ON -DCURL_DISABLE_TELNET=ON -DCURL_DISABLE_TFTP=ON -DCURL_DISABLE_POP3=ON -DCURL_DISABLE_IMAP=ON -DCURL_DISABLE_SMTP=ON -DCURL_DISABLE_GOPHER=ON -DCURL_DISABLE_MQTT=ON -DCURL_CA_BUNDLE=none -DCURL_CA_PATH=none\n"
                "    BUILD_BYPRODUCTS \"${CROSSA_CURL_INSTALL}/lib/libcurl.a\"\n"
                ")\n\n"
                "add_custom_target(crossa_ios_dependencies ALL DEPENDS crossa_ios_curl)\n";
        }

        // Writes Swift Package manifests for local development and release publication.
        static void writePackageWrapper(const filesystem::path& directory) {
            writeFile(directory / "Package.swift",
                "// swift-tools-version: 6.0\n"
                "import PackageDescription\n\n"
                "let package = Package(name: \"Crossa\", products: [.library(name: \"Crossa\", targets: [\"Crossa\"])], targets: [.binaryTarget(name: \"Crossa\", path: \"Artifacts/Crossa.xcframework\")])\n");
            writeFile(directory / "Package.swift.release.template",
                "// swift-tools-version: 6.0\n"
                "import PackageDescription\n\n"
                "// Generated Crossa mobile SDKs are project-specific. Publish this ZIP from the\n"
                "// repository that owns the generated API, not as a universal Crossa runtime.\n"
                "let package = Package(\n"
                "    name: \"Crossa\",\n"
                "    platforms: [\n"
                "        .iOS(\"" + IosBuildRequirements::minimumDeploymentTarget() + "\")\n"
                "    ],\n"
                "    products: [\n"
                "        .library(name: \"Crossa\", targets: [\"Crossa\"])\n"
                "    ],\n"
                "    targets: [\n"
                "        .binaryTarget(\n"
                "            name: \"Crossa\",\n"
                "            url: \"<CROSSA_PACKAGE_BASE_URL>/v<CROSSA_VERSION>/Crossa.xcframework.zip\",\n"
                "            checksum: \"<CROSSA_XCFRAMEWORK_CHECKSUM>\"\n"
                "        )\n"
                "    ]\n"
                ")\n");
        }

        // Writes stable artifact metadata input without volatile build-machine values.
        static void writeManifestTemplate(
            const filesystem::path& directory,
            const vector<const compiler::ir::Program*>& programs
        ) {
            size_t declarations = 0;
            for (const compiler::ir::Program* program : programs) {
                declarations += program == nullptr ? 0 : program->getDeclarations().size();
            }
            writeFile(directory / "Metadata" / "artifact-manifest.json",
                "{\n"
                "  \"compilerVersion\": \"0.1.0\",\n"
                "  \"languageVersion\": \"foundation\",\n"
                "  \"irVersion\": \"1\",\n"
                "  \"runtimeAbiVersion\": 1,\n"
                "  \"runtimeVersion\": \"0.1.0\",\n"
                "  \"module\": \"Crossa\",\n"
                "  \"deploymentTarget\": \"" +
                IosBuildRequirements::minimumDeploymentTarget() + "\",\n"
                "  \"nativeDependency\": { \"curl\": \"" +
                IosBuildRequirements::curlVersion() + "\" },\n"
                "  \"linkedDeclarationCount\": " + to_string(declarations) + "\n"
                "}\n");
        }
    };

    // Writes the complete iOS framework build project to the selected output directory.
    void IosProjectGenerator::generate(
        const vector<compiler::generators::swift::SwiftGeneratedSource>& swiftSources,
        const vector<const compiler::ir::Program*>& programs,
        const filesystem::path& outputDirectory
    ) const {
        IosProjectWriter::generate(swiftSources, programs, outputDirectory);
    }

    // Writes SwiftPM binary-package files for one archived XCFramework artifact.
    void IosProjectGenerator::writeBinaryPackage(
        const filesystem::path& artifactRoot,
        const string& packageVersion,
        const optional<string>& packageBaseUrl
    ) {
        IosProjectWriter::writeBinaryPackage(
            artifactRoot,
            packageVersion,
            packageBaseUrl
        );
    }

}
