#include "crossa/runtime/NativeRuntime.h"

#include <stdexcept>
#include <utility>

using namespace std;

namespace crossa::runtime {

    // Creates one runtime over immutable program and optional configuration IR.
    NativeRuntime::NativeRuntime(
        compiler::ir::Program program,
        const compiler::ir::Program* configurationProgram,
        const utils::Log& log
    )
        : program_(std::move(program)),
          log_(log),
          configuration_(RuntimeConfiguration::load(program_, configurationProgram)),
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

    // Starts one fire-and-forget operation and retains its cancellation lifecycle.
    uint64_t NativeRuntime::startAsync(
        uint64_t operationId,
        vector<RuntimeValue> arguments
    ) {
        return retainOperation(invokeAsync(operationId, std::move(arguments)));
    }

    // Starts one completion operation and retains its cancellation lifecycle.
    uint64_t NativeRuntime::startAsyncAfter(
        uint64_t operationId,
        vector<RuntimeValue> arguments,
        function<void(CrossaState<CrossaResultHandle>)> completion
    ) {
        return retainOperation(invokeAsyncAfter(
            operationId,
            std::move(arguments),
            std::move(completion)
        ));
    }

    // Requests cancellation for one active generated operation.
    bool NativeRuntime::cancelOperation(uint64_t operation) noexcept {
        lock_guard<mutex> lock(operationMutex_);
        const auto found = activeOperations_.find(operation);
        return found != activeOperations_.end() && found->second.cancel();
    }

    // Releases one generated operation lifecycle after native completion.
    bool NativeRuntime::releaseOperation(uint64_t operation) noexcept {
        lock_guard<mutex> lock(operationMutex_);
        return activeOperations_.erase(operation) == 1;
    }

    // Returns the context that owns result handles returned to platform bindings.
    bindings::sharedabi::CrossaRuntimeContext&
    NativeRuntime::resultContext() noexcept {
        return resultContext_;
    }

    // Stops the shared scheduler before runtime-owned state is released.
    void NativeRuntime::shutdown() {
        scheduler_.shutdown();
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

    // Stores one request handle behind a non-reused native operation identifier.
    uint64_t NativeRuntime::retainOperation(RequestHandle request) {
        lock_guard<mutex> lock(operationMutex_);
        if (nextOperation_ == 0) {
            throw runtime_error("Native operation handle capacity exhausted.");
        }
        const uint64_t operation = nextOperation_++;
        activeOperations_.emplace(operation, std::move(request));
        return operation;
    }

}
