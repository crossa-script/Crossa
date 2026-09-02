#pragma once

#include <cstdint>
#include <functional>
#include <mutex>
#include <optional>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "crossa/bindings/shared-abi/CrossaRuntimeContext.h"
#include "crossa/compiler/ir/Program.h"
#include "crossa/network/NetworkEngine.h"
#include "crossa/runtime/IrInterpreter.h"
#include "crossa/runtime/NativeOperationCatalog.h"
#include "crossa/runtime/RuntimeConfiguration.h"

namespace crossa::runtime {

// Owns native execution services and exposes generated operation IDs to bindings.
class NativeRuntime final {
public:
    // Creates one runtime over immutable program and optional configuration IR.
    NativeRuntime(
        compiler::ir::Program program,
        const compiler::ir::Program* configurationProgram,
        const utils::Log& log
    );

    ~NativeRuntime();

    NativeRuntime(const NativeRuntime&) = delete;
    NativeRuntime& operator=(const NativeRuntime&) = delete;

    // Invokes a generated fire-and-forget operation using the shared scheduler.
    [[nodiscard]] RequestHandle invokeAsync(
        std::uint64_t operationId,
        std::vector<RuntimeValue> arguments
    );

    // Invokes a generated completion operation and delivers exactly one result handle state.
    [[nodiscard]] RequestHandle invokeAsyncAfter(
        std::uint64_t operationId,
        std::vector<RuntimeValue> arguments,
        std::function<void(CrossaState<CrossaResultHandle>)> completion
    );

    // Starts one fire-and-forget operation and retains its cancellation lifecycle.
    [[nodiscard]] std::uint64_t startAsync(
        std::uint64_t operationId,
        std::vector<RuntimeValue> arguments
    );

    // Starts one completion operation and retains its cancellation lifecycle.
    [[nodiscard]] std::uint64_t startAsyncAfter(
        std::uint64_t operationId,
        std::vector<RuntimeValue> arguments,
        std::function<void(CrossaState<CrossaResultHandle>)> completion
    );

    // Requests cancellation for one active generated operation.
    [[nodiscard]] bool cancelOperation(std::uint64_t operation) noexcept;

    // Releases one generated operation lifecycle after native completion.
    [[nodiscard]] bool releaseOperation(std::uint64_t operation) noexcept;

    // Returns the context that owns result handles returned to platform bindings.
    [[nodiscard]] bindings::sharedabi::CrossaRuntimeContext& resultContext() noexcept;

    // Stops the scheduler and prevents future native operation execution.
    void shutdown();

    [[nodiscard]] bool isWorkerThread() const noexcept;

private:
    enum class Lifecycle {
        Running,
        ShutdownRequested,
        Stopped
    };

    // Resolves a generated operation ID or raises a native runtime error.
    [[nodiscard]] const compiler::ir::IrFunctionDeclaration& requireOperation(
        std::uint64_t operationId
    ) const;

    [[nodiscard]] std::uint64_t reserveOperation(RequestHandle* request);

    void removeActiveOperation(std::uint64_t operation) noexcept;

    void deliverAsyncAfter(
        CrossaState<RuntimeValue> state,
        std::function<void(CrossaState<CrossaResultHandle>)> completion
    );

    compiler::ir::Program program_;
    const utils::Log& log_;
    RuntimeConfiguration configuration_;
    network::NetworkEngine networkEngine_;
    scheduler::TaskScheduler scheduler_;
    IrInterpreter interpreter_;
    NativeOperationCatalog operations_;
    bindings::sharedabi::CrossaRuntimeContext resultContext_;
    std::mutex operationMutex_;
    std::uint64_t nextOperation_ = 1;
    Lifecycle lifecycle_ = Lifecycle::Running;
    std::unordered_map<std::uint64_t, RequestHandle> operationHandles_;
    std::unordered_set<std::uint64_t> activeOperations_;
};

}
