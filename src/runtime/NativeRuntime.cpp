#include "crossa/runtime/NativeRuntime.h"

#include <stdexcept>
#include <utility>

#include "crossa/runtime/errors/CrossaException.h"

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

    NativeRuntime::~NativeRuntime() {
        shutdown();
    }

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
        RequestHandle requestHandle;
        return interpreter_.invokeAsyncAfterOperation(
            requireOperation(operationId),
            std::move(arguments),
            requestHandle,
            [this, completion = std::move(completion)](
                CrossaState<RuntimeValue> state
            ) mutable {
                deliverAsyncAfter(std::move(state), std::move(completion));
            }
        );
    }

    // Starts one fire-and-forget operation and retains its cancellation lifecycle.
    uint64_t NativeRuntime::startAsync(
        uint64_t operationId,
        vector<RuntimeValue> arguments
    ) {
        RequestHandle requestHandle;
        const uint64_t operation = reserveOperation(&requestHandle);
        try {
            (void)interpreter_.invokeAsyncOperation(
                requireOperation(operationId),
                std::move(arguments),
                requestHandle,
                [this, operation](CrossaState<RuntimeValue>) {
                    removeActiveOperation(operation);
                }
            );
            return operation;
        } catch (...) {
            (void)releaseOperation(operation);
            throw;
        }
    }

    // Starts one completion operation and retains its cancellation lifecycle.
    uint64_t NativeRuntime::startAsyncAfter(
        uint64_t operationId,
        vector<RuntimeValue> arguments,
        function<void(CrossaState<CrossaResultHandle>)> completion
    ) {
        RequestHandle requestHandle;
        const uint64_t operation = reserveOperation(&requestHandle);
        try {
            (void)interpreter_.invokeAsyncAfterOperation(
                requireOperation(operationId),
                std::move(arguments),
                requestHandle,
                [this, operation, completion = std::move(completion)](
                    CrossaState<RuntimeValue> state
                ) mutable {
                    removeActiveOperation(operation);
                    deliverAsyncAfter(
                        std::move(state),
                        std::move(completion)
                    );
                }
            );
            return operation;
        } catch (...) {
            (void)releaseOperation(operation);
            throw;
        }
    }

    // Requests cancellation for one active generated operation.
    bool NativeRuntime::cancelOperation(uint64_t operation) noexcept {
        lock_guard<mutex> lock(operationMutex_);
        const auto found = operationHandles_.find(operation);
        return found != operationHandles_.end() && found->second.cancel();
    }

    // Releases one generated operation lifecycle after native completion.
    bool NativeRuntime::releaseOperation(uint64_t operation) noexcept {
        lock_guard<mutex> lock(operationMutex_);
        activeOperations_.erase(operation);
        return operationHandles_.erase(operation) == 1;
    }

    // Returns the context that owns result handles returned to platform bindings.
    bindings::sharedabi::CrossaRuntimeContext&
    NativeRuntime::resultContext() noexcept {
        return resultContext_;
    }

    // Stops the shared scheduler before runtime-owned state is released.
    void NativeRuntime::shutdown() {
        bool requestCancellation = false;
        {
            lock_guard<mutex> lock(operationMutex_);
            if (lifecycle_ == Lifecycle::Running) {
                lifecycle_ = Lifecycle::ShutdownRequested;
                requestCancellation = true;
            }
            if (requestCancellation) {
                for (const auto& [operation, requestHandle] : operationHandles_) {
                    (void)operation;
                    (void)requestHandle.cancel();
                }
            }
        }
        scheduler_.requestShutdown();
        if (scheduler_.isWorkerThread()) {
            return;
        }
        scheduler_.awaitTermination();
        lock_guard<mutex> lock(operationMutex_);
        lifecycle_ = Lifecycle::Stopped;
        activeOperations_.clear();
    }

    bool NativeRuntime::isWorkerThread() const noexcept {
        return scheduler_.isWorkerThread();
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

    uint64_t NativeRuntime::reserveOperation(RequestHandle* request) {
        if (request == nullptr) {
            throw invalid_argument("Native operation request handle is required.");
        }
        lock_guard<mutex> lock(operationMutex_);
        if (lifecycle_ != Lifecycle::Running) {
            throw CrossaException(
                CrossaError::runtime("Native runtime is shutting down.")
            );
        }
        if (nextOperation_ == 0) {
            throw runtime_error("Native operation handle capacity exhausted.");
        }
        const uint64_t operation = nextOperation_++;
        operationHandles_.emplace(operation, *request);
        activeOperations_.insert(operation);
        return operation;
    }

    void NativeRuntime::removeActiveOperation(uint64_t operation) noexcept {
        lock_guard<mutex> lock(operationMutex_);
        activeOperations_.erase(operation);
        operationHandles_.erase(operation);
    }

    void NativeRuntime::deliverAsyncAfter(
        CrossaState<RuntimeValue> state,
        function<void(CrossaState<CrossaResultHandle>)> completion
    ) {
        if (!completion) {
            return;
        }
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

}
