#pragma once

#include <functional>
#include <optional>

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
        const compiler::ir::Program& program,
        const compiler::ir::Program* configurationProgram,
        const utils::Log& log
    );

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

    // Returns the context that owns result handles returned to platform bindings.
    [[nodiscard]] bindings::sharedabi::CrossaRuntimeContext& resultContext() noexcept;

private:
    // Resolves a generated operation ID or raises a native runtime error.
    [[nodiscard]] const compiler::ir::IrFunctionDeclaration& requireOperation(
        std::uint64_t operationId
    ) const;

    const compiler::ir::Program& program_;
    const utils::Log& log_;
    RuntimeConfiguration configuration_;
    network::NetworkEngine networkEngine_;
    scheduler::TaskScheduler scheduler_;
    IrInterpreter interpreter_;
    NativeOperationCatalog operations_;
    bindings::sharedabi::CrossaRuntimeContext resultContext_;
};

}
