#include "crossa/runtime/NativeRuntime.h"

#include <stdexcept>
#include <utility>

using namespace std;

namespace crossa::runtime {

    // Creates one runtime over immutable program and optional configuration IR.
    NativeRuntime::NativeRuntime(
        const compiler::ir::Program& program,
        const compiler::ir::Program* configurationProgram,
        const utils::Log& log
    )
        : program_(program),
          log_(log),
          configuration_(RuntimeConfiguration::load(program, configurationProgram)),
          networkEngine_(
              configuration_.getNetworkConfiguration(),
              log_,
              configuration_.getSchedulerOptions().getWorkerCount()
          ),
          scheduler_(configuration_.getSchedulerOptions(), log_),
          interpreter_(
              program_,
              log_,
              scheduler_,
              networkEngine_,
              configuration_.getNetworkConfiguration(),
              ExecutionMode::Run
          ),
          operations_(program_) {}

    // Invokes a generated fire-and-forget operation using the shared scheduler.
    RequestHandle NativeRuntime::invokeAsync(
        uint64_t operationId,
        vector<RuntimeValue> arguments
    ) {
        return interpreter_.invokeAsyncOperation(
            requireOperation(operationId),
            std::move(arguments)
        );
    }

    // Invokes a generated completion operation and delivers exactly one result handle state.
    RequestHandle NativeRuntime::invokeAsyncAfter(
        uint64_t operationId,
        vector<RuntimeValue> arguments,
        function<void(CrossaState<CrossaResultHandle>)> completion
    ) {
        return interpreter_.invokeAsyncAfterOperation(
            requireOperation(operationId),
            std::move(arguments),
            [this, completion = std::move(completion)](
                CrossaState<RuntimeValue> state
            ) mutable {
                if (state.isCancelled()) {
                    completion(CrossaState<CrossaResultHandle>::cancelled());
                    return;
                }
                if (state.isFailed()) {
                    completion(CrossaState<CrossaResultHandle>::failed(
                        state.getError()
                    ));
                    return;
                }
                const CrossaResultHandle result = resultContext_.retainResult(
                    state.takeData()
                );
                if (result == 0) {
                    completion(CrossaState<CrossaResultHandle>::failed(
                        CrossaError::runtime("Native result handle capacity exhausted.")
                    ));
                    return;
                }
                completion(CrossaState<CrossaResultHandle>::success(result));
            }
        );
    }

    // Returns the context that owns result handles returned to platform bindings.
    bindings::sharedabi::CrossaRuntimeContext&
    NativeRuntime::resultContext() noexcept {
        return resultContext_;
    }

    // Resolves a generated operation ID or raises a native runtime error.
    const compiler::ir::IrFunctionDeclaration& NativeRuntime::requireOperation(
        uint64_t operationId
    ) const {
        const compiler::ir::IrFunctionDeclaration* function = operations_.find(
            operationId
        );
        if (function == nullptr) {
            throw runtime_error("Unknown generated Crossa operation ID.");
        }
        return *function;
    }

}
