#include "crossa/bindings/shared-abi/CrossaAbi.h"

#include <memory>
#include <mutex>
#include <new>
#include <condition_variable>
#include <deque>
#include <limits>
#include <string>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

#include "crossa/bindings/shared-abi/CrossaRuntimeContext.h"
#include "crossa/bindings/shared-abi/CrossaAbiRuntimeFactory.h"
#include "crossa/runtime/NativeRuntime.h"
#include "crossa/runtime/objects/NativeList.h"
#include "crossa/runtime/objects/NativeModel.h"

using namespace std;

namespace crossa::bindings::sharedabi {

    // Owns validated opaque runtime handles and their native runtimes.
    class CrossaAbiRuntimeRegistry final {
    public:
        ~CrossaAbiRuntimeRegistry() {
            {
                lock_guard<mutex> lock(mutex_);
                for (auto& [handle, nativeRuntime] : runtimes_) {
                    (void)handle;
                    retired_.push_back(std::move(nativeRuntime));
                }
                runtimes_.clear();
                reaperStopping_ = true;
            }
            condition_.notify_all();
            if (reaper_.joinable()) {
                reaper_.join();
            }
        }

        // Removes one runtime after its scheduler has stopped using result storage.
        static void release(CrossaRuntimeHandle runtime) {
            CrossaAbiRuntimeRegistry& registry = instance();
            shared_ptr<runtime::NativeRuntime> nativeRuntime;
            bool deferred = false;
            {
                lock_guard<mutex> lock(registry.mutex_);
                const auto found = registry.runtimes_.find(runtime);
                if (found == registry.runtimes_.end()) return;
                nativeRuntime = found->second;
                registry.runtimes_.erase(found);
                if (nativeRuntime->isWorkerThread()) {
                    registry.retired_.push_back(nativeRuntime);
                    try {
                        registry.startReaperLocked();
                    } catch (...) {
                    }
                    deferred = true;
                }
            }
            nativeRuntime->shutdown();
            if (!deferred) {
                nativeRuntime.reset();
            }
        }

        static CrossaStatus createNative(
            compiler::ir::Program program,
            const compiler::ir::Program* configurationProgram,
            const utils::Log& log,
            CrossaRuntimeHandle* runtime
        ) {
            if (runtime == nullptr) return CrossaStatusInvalidArgument;
            shared_ptr<runtime::NativeRuntime> nativeRuntime;
            try {
                nativeRuntime = make_shared<runtime::NativeRuntime>(
                    std::move(program), configurationProgram, log
                );
            } catch (...) {
                *runtime = 0;
                return CrossaStatusInternalError;
            }
            lock_guard<mutex> lock(instance().mutex_);
            if (instance().nextHandle_ == 0) return CrossaStatusInternalError;
            const CrossaRuntimeHandle handle = instance().nextHandle_++;
            instance().runtimes_.emplace(handle, std::move(nativeRuntime));
            *runtime = handle;
            return CrossaStatusOk;
        }

        static shared_ptr<runtime::NativeRuntime> findNative(CrossaRuntimeHandle runtime) noexcept {
            try {
                lock_guard<mutex> lock(instance().mutex_);
                const auto found = instance().runtimes_.find(runtime);
                return found == instance().runtimes_.end() ? nullptr : found->second;
            } catch (...) {
                return nullptr;
            }
        }

    private:
        // Creates the process-local registry for the stable C ABI.
        CrossaAbiRuntimeRegistry() : nextHandle_(1) {}

        void startReaperLocked() {
            if (reaper_.joinable()) {
                condition_.notify_one();
                return;
            }
            reaper_ = thread([this]() { reapRetiredRuntimes(); });
        }

        void reapRetiredRuntimes() {
            while (true) {
                shared_ptr<runtime::NativeRuntime> nativeRuntime;
                {
                    unique_lock<mutex> lock(mutex_);
                    condition_.wait(lock, [this]() {
                        return reaperStopping_ || !retired_.empty();
                    });
                    if (retired_.empty() && reaperStopping_) {
                        return;
                    }
                    nativeRuntime = std::move(retired_.front());
                    retired_.pop_front();
                }
                nativeRuntime->shutdown();
            }
        }

        // Returns the sole registry instance used by ABI functions.
        static CrossaAbiRuntimeRegistry& instance() {
            static CrossaAbiRuntimeRegistry registry;
            return registry;
        }

        mutex mutex_;
        CrossaRuntimeHandle nextHandle_;
        unordered_map<CrossaRuntimeHandle, shared_ptr<runtime::NativeRuntime>>
            runtimes_;
        deque<shared_ptr<runtime::NativeRuntime>> retired_;
        condition_variable condition_;
        thread reaper_;
        bool reaperStopping_ = false;
    };

    // Resolves one result handle while preserving its native storage lifetime.
    class CrossaAbiValueAccess final {
    public:
        // Finds one retained result through a validated opaque runtime handle.
        static shared_ptr<const runtime::RuntimeValue> findResult(
            CrossaRuntimeHandle runtime,
            CrossaResultHandle result
        ) {
            const shared_ptr<runtime::NativeRuntime> nativeRuntime =
                CrossaAbiRuntimeRegistry::findNative(runtime);
            return nativeRuntime == nullptr ? nullptr :
                nativeRuntime->resultContext().findResult(result);
        }

        // Converts borrowed ABI arguments into owned runtime values for one call.
        static bool convertArguments(
            const CrossaAbiArgument* arguments,
            size_t argumentCount,
            vector<runtime::RuntimeValue>* values
        ) {
            if (values == nullptr || (arguments == nullptr && argumentCount != 0)) {
                return false;
            }
            values->clear();
            values->reserve(argumentCount);
            for (size_t index = 0; index < argumentCount; ++index) {
                const CrossaAbiArgument& argument = arguments[index];
                switch (argument.kind) {
                    case CrossaAbiArgumentInt:
                        if (argument.integerValue < numeric_limits<int32_t>::min() ||
                            argument.integerValue > numeric_limits<int32_t>::max()) {
                            return false;
                        }
                        values->push_back(runtime::RuntimeValue::createInt(
                            argument.integerValue
                        ));
                        break;
                    case CrossaAbiArgumentLong:
                        values->push_back(runtime::RuntimeValue::createLong(
                            argument.integerValue
                        ));
                        break;
                    case CrossaAbiArgumentDouble:
                        values->push_back(runtime::RuntimeValue::createDouble(
                            argument.doubleValue
                        ));
                        break;
                    case CrossaAbiArgumentBool:
                        values->push_back(runtime::RuntimeValue::createBool(
                            argument.integerValue != 0
                        ));
                        break;
                    case CrossaAbiArgumentString:
                        if (argument.stringValue.data == nullptr &&
                            argument.stringValue.size != 0) return false;
                        values->push_back(runtime::RuntimeValue::createString(string(
                            argument.stringValue.data == nullptr ? "" :
                                argument.stringValue.data,
                            argument.stringValue.size
                        )));
                        break;
                    default:
                        return false;
                }
            }
            return true;
        }

        // Resolves one model view encoded as a root or list-element index.
        static const runtime::NativeModel* findModel(
            const runtime::RuntimeValue& result,
            CrossaModelHandle model
        ) {
            if (model == 0) return nullptr;
            if (result.getKind() == runtime::RuntimeValueKind::Model) {
                return model == 1 ? &result.getModel() : nullptr;
            }
            if (result.getKind() != runtime::RuntimeValueKind::List) return nullptr;
            const size_t index = static_cast<size_t>(model - 1);
            if (index >= result.getList().getSize()) return nullptr;
            const runtime::RuntimeValue& value = result.getList().get(index);
            return value.getKind() == runtime::RuntimeValueKind::Model
                ? &value.getModel()
                : nullptr;
        }

        // Resolves one schema-order field from a native model view.
        static const runtime::RuntimeValue* findField(
            const runtime::RuntimeValue& result,
            CrossaModelHandle model,
            uint32_t field
        ) {
            const runtime::NativeModel* nativeModel = findModel(result, model);
            if (nativeModel == nullptr ||
                field >= nativeModel->getFields().size()) return nullptr;
            return &nativeModel->getFields()[field].second;
        }

        // Resolves one generated path without exposing native object addresses.
        static const runtime::RuntimeValue* findPath(
            const runtime::RuntimeValue& result,
            const CrossaAbiPathSegment* path,
            size_t pathCount
        ) {
            if (path == nullptr && pathCount != 0) return nullptr;
            const runtime::RuntimeValue* value = &result;
            for (size_t index = 0; index < pathCount; ++index) {
                const CrossaAbiPathSegment& segment = path[index];
                if (segment.kind == CrossaAbiPathField) {
                    if (value->getKind() != runtime::RuntimeValueKind::Model ||
                        segment.index >= value->getModel().getFields().size()) {
                        return nullptr;
                    }
                    value = &value->getModel().getFields()[segment.index].second;
                    continue;
                }
                if (segment.kind == CrossaAbiPathListElement) {
                    if (value->getKind() != runtime::RuntimeValueKind::List ||
                        segment.index >= value->getList().getSize()) {
                        return nullptr;
                    }
                    value = &value->getList().get(segment.index);
                    continue;
                }
                return nullptr;
            }
            return value;
        }

        // Maps one native runtime value type to its stable ABI category.
        static CrossaValueKind mapKind(runtime::RuntimeValueKind kind) {
            switch (kind) {
                case runtime::RuntimeValueKind::Unit: return CrossaValueUnit;
                case runtime::RuntimeValueKind::Int: return CrossaValueInt;
                case runtime::RuntimeValueKind::Long: return CrossaValueLong;
                case runtime::RuntimeValueKind::Double: return CrossaValueDouble;
                case runtime::RuntimeValueKind::String: return CrossaValueString;
                case runtime::RuntimeValueKind::Bool: return CrossaValueBool;
                case runtime::RuntimeValueKind::Model: return CrossaValueModel;
                case runtime::RuntimeValueKind::List: return CrossaValueList;
                case runtime::RuntimeValueKind::Json: return CrossaValueString;
            }
            return CrossaValueUnit;
        }
    };

}

namespace crossa::bindings::sharedabi {

    CrossaStatus CrossaAbiRuntimeFactory::create(
        compiler::ir::Program program,
        const compiler::ir::Program* configurationProgram,
        const utils::Log& log,
        CrossaRuntimeHandle* runtime
    ) {
        return CrossaAbiRuntimeRegistry::createNative(
            std::move(program), configurationProgram, log, runtime
        );
    }

}

extern "C" {

    // Rejects context-only construction because results belong to NativeRuntime.
    CrossaStatus crossaCreateRuntime(CrossaRuntimeHandle* runtime) {
        if (runtime != nullptr) *runtime = 0;
        return CrossaStatusInvalidArgument;
    }

    // Destroys a runtime after invalidating all results and errors it owns.
    void crossaReleaseRuntime(CrossaRuntimeHandle runtime) {
        try {
            crossa::bindings::sharedabi::CrossaAbiRuntimeRegistry::release(runtime);
        } catch (...) {
        }
    }

    // Stops accepted work while preserving runtime-owned result handles.
    CrossaStatus crossaRuntimeShutdown(CrossaRuntimeHandle runtime) {
        try {
            const auto nativeRuntime =
                crossa::bindings::sharedabi::CrossaAbiRuntimeRegistry::findNative(runtime);
            if (nativeRuntime == nullptr) return CrossaStatusInvalidHandle;
            nativeRuntime->shutdown();
            return CrossaStatusOk;
        } catch (...) {
            return CrossaStatusInternalError;
        }
    }

    // Starts an Async operation and returns its cancellable lifecycle handle.
    CrossaStatus crossaInvokeAsync(
        CrossaRuntimeHandle runtime,
        CrossaOperationId operation,
        const CrossaAbiArgument* arguments,
        size_t argumentCount,
        CrossaOperationHandle* invocation
    ) {
        if (invocation == nullptr) return CrossaStatusInvalidArgument;
        *invocation = 0;
        try {
            const auto nativeRuntime =
                crossa::bindings::sharedabi::CrossaAbiRuntimeRegistry::findNative(runtime);
            if (nativeRuntime == nullptr) return CrossaStatusInvalidHandle;
            vector<crossa::runtime::RuntimeValue> values;
            if (!crossa::bindings::sharedabi::CrossaAbiValueAccess::convertArguments(
                    arguments, argumentCount, &values
                )) return CrossaStatusInvalidArgument;
            *invocation = nativeRuntime->startAsync(operation, std::move(values));
            return CrossaStatusOk;
        } catch (...) {
            return CrossaStatusInvalidArgument;
        }
    }

    // Starts an AsyncAfter operation and bridges its native terminal state.
    CrossaStatus crossaInvokeAsyncAfter(
        CrossaRuntimeHandle runtime,
        CrossaOperationId operation,
        const CrossaAbiArgument* arguments,
        size_t argumentCount,
        CrossaAbiCompletion completion,
        void* userData,
        CrossaOperationHandle* invocation
    ) {
        if (completion == nullptr || invocation == nullptr) {
            return CrossaStatusInvalidArgument;
        }
        *invocation = 0;
        try {
            const auto nativeRuntime =
                crossa::bindings::sharedabi::CrossaAbiRuntimeRegistry::findNative(runtime);
            if (nativeRuntime == nullptr) return CrossaStatusInvalidHandle;
            vector<crossa::runtime::RuntimeValue> values;
            if (!crossa::bindings::sharedabi::CrossaAbiValueAccess::convertArguments(
                    arguments, argumentCount, &values
                )) return CrossaStatusInvalidArgument;
            *invocation = nativeRuntime->startAsyncAfter(
                operation,
                std::move(values),
                [nativeRuntime, completion, userData](
                    crossa::runtime::CrossaState<CrossaResultHandle> state
                ) {
                    CrossaResultHandle result = 0;
                    CrossaErrorHandle error = 0;
                    bool callbackAttempted = false;
                    try {
                        CrossaAbiCompletionKind kind = CrossaAbiCompletionCancelled;
                        if (state.isSuccess()) {
                            kind = CrossaAbiCompletionSuccess;
                            result = state.getData();
                        } else if (state.isFailed()) {
                            kind = CrossaAbiCompletionFailed;
                            error = nativeRuntime->resultContext().retainError(
                                state.getError()
                            );
                        }
                        callbackAttempted = true;
                        completion(userData, kind, result, error);
                    } catch (...) {
                        if (result != 0) {
                            (void)nativeRuntime->resultContext().releaseResult(result);
                        }
                        if (error != 0) {
                            (void)nativeRuntime->resultContext().releaseError(error);
                        }
                        if (!callbackAttempted) {
                            try {
                                completion(
                                    userData,
                                    CrossaAbiCompletionFailed,
                                    0,
                                    0
                                );
                            } catch (...) {
                            }
                        }
                    }
                }
            );
            return CrossaStatusOk;
        } catch (...) {
            return CrossaStatusInvalidArgument;
        }
    }

    // Requests cancellation for an accepted native operation.
    CrossaStatus crossaCancelOperation(
        CrossaRuntimeHandle runtime,
        CrossaOperationHandle operation
    ) {
        try {
            const auto nativeRuntime =
                crossa::bindings::sharedabi::CrossaAbiRuntimeRegistry::findNative(runtime);
            if (nativeRuntime == nullptr) return CrossaStatusInvalidHandle;
            return nativeRuntime->cancelOperation(operation)
                ? CrossaStatusOk : CrossaStatusInvalidHandle;
        } catch (...) {
            return CrossaStatusInternalError;
        }
    }

    // Releases one caller-owned native operation lifecycle.
    CrossaStatus crossaReleaseOperation(
        CrossaRuntimeHandle runtime,
        CrossaOperationHandle operation
    ) {
        try {
            const auto nativeRuntime =
                crossa::bindings::sharedabi::CrossaAbiRuntimeRegistry::findNative(runtime);
            if (nativeRuntime == nullptr) return CrossaStatusInvalidHandle;
            return nativeRuntime->releaseOperation(operation)
                ? CrossaStatusOk : CrossaStatusInvalidHandle;
        } catch (...) {
            return CrossaStatusInternalError;
        }
    }

    // Releases one result and makes future accesses fail deterministically.
    CrossaStatus crossaReleaseResult(CrossaRuntimeHandle runtime, CrossaResultHandle result) {
        try {
            const auto nativeRuntime =
                crossa::bindings::sharedabi::CrossaAbiRuntimeRegistry::findNative(runtime);
            return nativeRuntime == nullptr ? CrossaStatusInvalidHandle :
                nativeRuntime->resultContext().releaseResult(result);
        } catch (...) {
            return CrossaStatusInternalError;
        }
    }

    // Releases one runtime-owned error handle.
    CrossaStatus crossaReleaseError(CrossaRuntimeHandle runtime, CrossaErrorHandle error) {
        try {
            const auto nativeRuntime =
                crossa::bindings::sharedabi::CrossaAbiRuntimeRegistry::findNative(runtime);
            return nativeRuntime == nullptr ? CrossaStatusInvalidHandle :
                nativeRuntime->resultContext().releaseError(error);
        } catch (...) {
            return CrossaStatusInternalError;
        }
    }

    // Reads a borrowed runtime-owned error message.
    CrossaStatus crossaGetErrorMessage(
        CrossaRuntimeHandle runtime,
        CrossaErrorHandle error,
        CrossaStringView* value
    ) {
        try {
            if (value == nullptr) return CrossaStatusInvalidArgument;
            const auto nativeRuntime =
                crossa::bindings::sharedabi::CrossaAbiRuntimeRegistry::findNative(runtime);
            if (nativeRuntime == nullptr) return CrossaStatusInvalidHandle;
            const auto nativeError = nativeRuntime->resultContext().findError(error);
            if (nativeError == nullptr) return CrossaStatusInvalidHandle;
            value->data = nativeError->getMessage().data();
            value->size = nativeError->getMessage().size();
            return CrossaStatusOk;
        } catch (...) {
            return CrossaStatusInternalError;
        }
    }

    // Reads stable error metadata from the canonical runtime result context.
    CrossaStatus crossaGetErrorMetadata(
        CrossaRuntimeHandle runtime,
        CrossaErrorHandle error,
        int32_t* domain,
        int32_t* code,
        uint8_t* retryable
    ) {
        try {
            if (domain == nullptr || code == nullptr || retryable == nullptr) {
                return CrossaStatusInvalidArgument;
            }
            const auto nativeRuntime =
                crossa::bindings::sharedabi::CrossaAbiRuntimeRegistry::findNative(runtime);
            if (nativeRuntime == nullptr) return CrossaStatusInvalidHandle;
            const auto nativeError = nativeRuntime->resultContext().findError(error);
            if (nativeError == nullptr) return CrossaStatusInvalidHandle;
            *domain = static_cast<int32_t>(nativeError->getDomain());
            *code = static_cast<int32_t>(nativeError->getCode());
            *retryable = nativeError->isRetryable() ? 1 : 0;
            return CrossaStatusOk;
        } catch (...) {
            return CrossaStatusInternalError;
        }
    }

    // Reads a result category without exposing native implementation types.
    CrossaStatus crossaGetResultKind(CrossaRuntimeHandle runtime, CrossaResultHandle result, CrossaValueKind* kind) {
        if (kind == nullptr) return CrossaStatusInvalidArgument;
        const auto value = crossa::bindings::sharedabi::CrossaAbiValueAccess::findResult(runtime, result);
        if (value == nullptr) return CrossaStatusInvalidHandle;
        *kind = crossa::bindings::sharedabi::CrossaAbiValueAccess::mapKind(value->getKind());
        return CrossaStatusOk;
    }

    // Returns the element count for a native list result.
    CrossaStatus crossaGetListSize(CrossaRuntimeHandle runtime, CrossaResultHandle result, size_t* size) {
        if (size == nullptr) return CrossaStatusInvalidArgument;
        const auto value = crossa::bindings::sharedabi::CrossaAbiValueAccess::findResult(runtime, result);
        if (value == nullptr) return CrossaStatusInvalidHandle;
        if (value->getKind() != crossa::runtime::RuntimeValueKind::List) return CrossaStatusTypeMismatch;
        *size = value->getList().getSize();
        return CrossaStatusOk;
    }

    // Returns a borrowed list-model view encoded relative to the retained result.
    CrossaStatus crossaGetListModel(CrossaRuntimeHandle runtime, CrossaResultHandle result, size_t index, CrossaModelHandle* model) {
        if (model == nullptr) return CrossaStatusInvalidArgument;
        const auto value = crossa::bindings::sharedabi::CrossaAbiValueAccess::findResult(runtime, result);
        if (value == nullptr) return CrossaStatusInvalidHandle;
        if (value->getKind() != crossa::runtime::RuntimeValueKind::List) return CrossaStatusTypeMismatch;
        if (index >= value->getList().getSize() || value->getList().get(index).getKind() != crossa::runtime::RuntimeValueKind::Model) return CrossaStatusOutOfBounds;
        *model = static_cast<CrossaModelHandle>(index + 1);
        return CrossaStatusOk;
    }

    // Returns the root model view for a retained model result.
    CrossaStatus crossaGetRootModel(CrossaRuntimeHandle runtime, CrossaResultHandle result, CrossaModelHandle* model) {
        if (model == nullptr) return CrossaStatusInvalidArgument;
        const auto value = crossa::bindings::sharedabi::CrossaAbiValueAccess::findResult(runtime, result);
        if (value == nullptr) return CrossaStatusInvalidHandle;
        if (value->getKind() != crossa::runtime::RuntimeValueKind::Model) return CrossaStatusTypeMismatch;
        *model = 1;
        return CrossaStatusOk;
    }

    // Reads one Int root result through its retained opaque handle.
    CrossaStatus crossaGetResultInt(CrossaRuntimeHandle runtime, CrossaResultHandle result, int32_t* value) {
        if (value == nullptr) return CrossaStatusInvalidArgument;
        const auto root = crossa::bindings::sharedabi::CrossaAbiValueAccess::findResult(runtime, result);
        if (root == nullptr) return CrossaStatusInvalidHandle;
        if (root->getKind() != crossa::runtime::RuntimeValueKind::Int) return CrossaStatusTypeMismatch;
        if (root->getInt() < numeric_limits<int32_t>::min() ||
            root->getInt() > numeric_limits<int32_t>::max()) {
            return CrossaStatusOutOfBounds;
        }
        *value = static_cast<int32_t>(root->getInt());
        return CrossaStatusOk;
    }

    // Reads one Long root result through its retained opaque handle.
    CrossaStatus crossaGetResultLong(CrossaRuntimeHandle runtime, CrossaResultHandle result, int64_t* value) {
        if (value == nullptr) return CrossaStatusInvalidArgument;
        const auto root = crossa::bindings::sharedabi::CrossaAbiValueAccess::findResult(runtime, result);
        if (root == nullptr) return CrossaStatusInvalidHandle;
        if (root->getKind() != crossa::runtime::RuntimeValueKind::Long) return CrossaStatusTypeMismatch;
        *value = root->getLong();
        return CrossaStatusOk;
    }

    // Reads one Double root result through its retained opaque handle.
    CrossaStatus crossaGetResultDouble(CrossaRuntimeHandle runtime, CrossaResultHandle result, double* value) {
        if (value == nullptr) return CrossaStatusInvalidArgument;
        const auto root = crossa::bindings::sharedabi::CrossaAbiValueAccess::findResult(runtime, result);
        if (root == nullptr) return CrossaStatusInvalidHandle;
        if (root->getKind() != crossa::runtime::RuntimeValueKind::Double) return CrossaStatusTypeMismatch;
        *value = root->getDouble();
        return CrossaStatusOk;
    }

    // Reads one String root result as a view valid until result release.
    CrossaStatus crossaGetResultString(CrossaRuntimeHandle runtime, CrossaResultHandle result, CrossaStringView* value) {
        if (value == nullptr) return CrossaStatusInvalidArgument;
        const auto root = crossa::bindings::sharedabi::CrossaAbiValueAccess::findResult(runtime, result);
        if (root == nullptr) return CrossaStatusInvalidHandle;
        if (root->getKind() != crossa::runtime::RuntimeValueKind::String) return CrossaStatusTypeMismatch;
        value->data = root->getString().data();
        value->size = root->getString().size();
        return CrossaStatusOk;
    }

    // Reads one Bool root result through its retained opaque handle.
    CrossaStatus crossaGetResultBool(CrossaRuntimeHandle runtime, CrossaResultHandle result, uint8_t* value) {
        if (value == nullptr) return CrossaStatusInvalidArgument;
        const auto root = crossa::bindings::sharedabi::CrossaAbiValueAccess::findResult(runtime, result);
        if (root == nullptr) return CrossaStatusInvalidHandle;
        if (root->getKind() != crossa::runtime::RuntimeValueKind::Bool) return CrossaStatusTypeMismatch;
        *value = root->getBool() ? 1 : 0;
        return CrossaStatusOk;
    }

    // Reads one Int model field by stable generated index.
    CrossaStatus crossaGetModelInt(CrossaRuntimeHandle runtime, CrossaResultHandle result, CrossaModelHandle model, uint32_t field, int32_t* value) {
        if (value == nullptr) return CrossaStatusInvalidArgument;
        const auto root = crossa::bindings::sharedabi::CrossaAbiValueAccess::findResult(runtime, result);
        const auto* member = root == nullptr ? nullptr : crossa::bindings::sharedabi::CrossaAbiValueAccess::findField(*root, model, field);
        if (root == nullptr) return CrossaStatusInvalidHandle;
        if (member == nullptr) return CrossaStatusOutOfBounds;
        if (member->getKind() != crossa::runtime::RuntimeValueKind::Int) return CrossaStatusTypeMismatch;
        if (member->getInt() < numeric_limits<int32_t>::min() ||
            member->getInt() > numeric_limits<int32_t>::max()) {
            return CrossaStatusOutOfBounds;
        }
        *value = static_cast<int32_t>(member->getInt());
        return CrossaStatusOk;
    }

    // Reads one Long model field by stable generated index.
    CrossaStatus crossaGetModelLong(CrossaRuntimeHandle runtime, CrossaResultHandle result, CrossaModelHandle model, uint32_t field, int64_t* value) {
        if (value == nullptr) return CrossaStatusInvalidArgument;
        const auto root = crossa::bindings::sharedabi::CrossaAbiValueAccess::findResult(runtime, result);
        const auto* member = root == nullptr ? nullptr : crossa::bindings::sharedabi::CrossaAbiValueAccess::findField(*root, model, field);
        if (root == nullptr) return CrossaStatusInvalidHandle;
        if (member == nullptr) return CrossaStatusOutOfBounds;
        if (member->getKind() != crossa::runtime::RuntimeValueKind::Long) return CrossaStatusTypeMismatch;
        *value = member->getLong();
        return CrossaStatusOk;
    }

    // Reads one Double model field by stable generated index.
    CrossaStatus crossaGetModelDouble(CrossaRuntimeHandle runtime, CrossaResultHandle result, CrossaModelHandle model, uint32_t field, double* value) {
        if (value == nullptr) return CrossaStatusInvalidArgument;
        const auto root = crossa::bindings::sharedabi::CrossaAbiValueAccess::findResult(runtime, result);
        const auto* member = root == nullptr ? nullptr : crossa::bindings::sharedabi::CrossaAbiValueAccess::findField(*root, model, field);
        if (root == nullptr) return CrossaStatusInvalidHandle;
        if (member == nullptr) return CrossaStatusOutOfBounds;
        if (member->getKind() != crossa::runtime::RuntimeValueKind::Double) return CrossaStatusTypeMismatch;
        *value = member->getDouble();
        return CrossaStatusOk;
    }

    // Reads one String model field by stable generated index.
    CrossaStatus crossaGetModelString(CrossaRuntimeHandle runtime, CrossaResultHandle result, CrossaModelHandle model, uint32_t field, CrossaStringView* value) {
        if (value == nullptr) return CrossaStatusInvalidArgument;
        const auto root = crossa::bindings::sharedabi::CrossaAbiValueAccess::findResult(runtime, result);
        const auto* member = root == nullptr ? nullptr : crossa::bindings::sharedabi::CrossaAbiValueAccess::findField(*root, model, field);
        if (root == nullptr) return CrossaStatusInvalidHandle;
        if (member == nullptr) return CrossaStatusOutOfBounds;
        if (member->getKind() != crossa::runtime::RuntimeValueKind::String) return CrossaStatusTypeMismatch;
        value->data = member->getString().data();
        value->size = member->getString().size();
        return CrossaStatusOk;
    }

    // Reads one Bool model field by stable generated index.
    CrossaStatus crossaGetModelBool(CrossaRuntimeHandle runtime, CrossaResultHandle result, CrossaModelHandle model, uint32_t field, uint8_t* value) {
        if (value == nullptr) return CrossaStatusInvalidArgument;
        const auto root = crossa::bindings::sharedabi::CrossaAbiValueAccess::findResult(runtime, result);
        const auto* member = root == nullptr ? nullptr : crossa::bindings::sharedabi::CrossaAbiValueAccess::findField(*root, model, field);
        if (root == nullptr) return CrossaStatusInvalidHandle;
        if (member == nullptr) return CrossaStatusOutOfBounds;
        if (member->getKind() != crossa::runtime::RuntimeValueKind::Bool) return CrossaStatusTypeMismatch;
        *value = member->getBool() ? 1 : 0;
        return CrossaStatusOk;
    }

    // Returns the type of one generated path inside a retained result tree.
    CrossaStatus crossaGetPathKind(
        CrossaRuntimeHandle runtime,
        CrossaResultHandle result,
        const CrossaAbiPathSegment* path,
        size_t pathCount,
        CrossaValueKind* kind
    ) {
        if (kind == nullptr) return CrossaStatusInvalidArgument;
        const auto root = crossa::bindings::sharedabi::CrossaAbiValueAccess::findResult(
            runtime, result
        );
        if (root == nullptr) return CrossaStatusInvalidHandle;
        const auto* value = crossa::bindings::sharedabi::CrossaAbiValueAccess::findPath(
            *root, path, pathCount
        );
        if (value == nullptr) return CrossaStatusOutOfBounds;
        *kind = crossa::bindings::sharedabi::CrossaAbiValueAccess::mapKind(
            value->getKind()
        );
        return CrossaStatusOk;
    }

    // Returns the element count for a list resolved through a generated path.
    CrossaStatus crossaGetPathListSize(
        CrossaRuntimeHandle runtime,
        CrossaResultHandle result,
        const CrossaAbiPathSegment* path,
        size_t pathCount,
        size_t* size
    ) {
        if (size == nullptr) return CrossaStatusInvalidArgument;
        const auto root = crossa::bindings::sharedabi::CrossaAbiValueAccess::findResult(
            runtime, result
        );
        if (root == nullptr) return CrossaStatusInvalidHandle;
        const auto* value = crossa::bindings::sharedabi::CrossaAbiValueAccess::findPath(
            *root, path, pathCount
        );
        if (value == nullptr) return CrossaStatusOutOfBounds;
        if (value->getKind() != crossa::runtime::RuntimeValueKind::List) {
            return CrossaStatusTypeMismatch;
        }
        *size = value->getList().getSize();
        return CrossaStatusOk;
    }

    // Reads one Int value through a generated result path.
    CrossaStatus crossaGetPathInt(
        CrossaRuntimeHandle runtime,
        CrossaResultHandle result,
        const CrossaAbiPathSegment* path,
        size_t pathCount,
        int32_t* value
    ) {
        if (value == nullptr) return CrossaStatusInvalidArgument;
        const auto root = crossa::bindings::sharedabi::CrossaAbiValueAccess::findResult(
            runtime, result
        );
        if (root == nullptr) return CrossaStatusInvalidHandle;
        const auto* member = crossa::bindings::sharedabi::CrossaAbiValueAccess::findPath(
            *root, path, pathCount
        );
        if (member == nullptr) return CrossaStatusOutOfBounds;
        if (member->getKind() != crossa::runtime::RuntimeValueKind::Int) {
            return CrossaStatusTypeMismatch;
        }
        if (member->getInt() < numeric_limits<int32_t>::min() ||
            member->getInt() > numeric_limits<int32_t>::max()) {
            return CrossaStatusOutOfBounds;
        }
        *value = static_cast<int32_t>(member->getInt());
        return CrossaStatusOk;
    }

    // Reads one Long value through a generated result path.
    CrossaStatus crossaGetPathLong(
        CrossaRuntimeHandle runtime,
        CrossaResultHandle result,
        const CrossaAbiPathSegment* path,
        size_t pathCount,
        int64_t* value
    ) {
        if (value == nullptr) return CrossaStatusInvalidArgument;
        const auto root = crossa::bindings::sharedabi::CrossaAbiValueAccess::findResult(
            runtime, result
        );
        if (root == nullptr) return CrossaStatusInvalidHandle;
        const auto* member = crossa::bindings::sharedabi::CrossaAbiValueAccess::findPath(
            *root, path, pathCount
        );
        if (member == nullptr) return CrossaStatusOutOfBounds;
        if (member->getKind() != crossa::runtime::RuntimeValueKind::Long) {
            return CrossaStatusTypeMismatch;
        }
        *value = member->getLong();
        return CrossaStatusOk;
    }

    // Reads one Double value through a generated result path.
    CrossaStatus crossaGetPathDouble(
        CrossaRuntimeHandle runtime,
        CrossaResultHandle result,
        const CrossaAbiPathSegment* path,
        size_t pathCount,
        double* value
    ) {
        if (value == nullptr) return CrossaStatusInvalidArgument;
        const auto root = crossa::bindings::sharedabi::CrossaAbiValueAccess::findResult(
            runtime, result
        );
        if (root == nullptr) return CrossaStatusInvalidHandle;
        const auto* member = crossa::bindings::sharedabi::CrossaAbiValueAccess::findPath(
            *root, path, pathCount
        );
        if (member == nullptr) return CrossaStatusOutOfBounds;
        if (member->getKind() != crossa::runtime::RuntimeValueKind::Double) {
            return CrossaStatusTypeMismatch;
        }
        *value = member->getDouble();
        return CrossaStatusOk;
    }

    // Reads one String view through a generated result path.
    CrossaStatus crossaGetPathString(
        CrossaRuntimeHandle runtime,
        CrossaResultHandle result,
        const CrossaAbiPathSegment* path,
        size_t pathCount,
        CrossaStringView* value
    ) {
        if (value == nullptr) return CrossaStatusInvalidArgument;
        const auto root = crossa::bindings::sharedabi::CrossaAbiValueAccess::findResult(
            runtime, result
        );
        if (root == nullptr) return CrossaStatusInvalidHandle;
        const auto* member = crossa::bindings::sharedabi::CrossaAbiValueAccess::findPath(
            *root, path, pathCount
        );
        if (member == nullptr) return CrossaStatusOutOfBounds;
        if (member->getKind() != crossa::runtime::RuntimeValueKind::String) {
            return CrossaStatusTypeMismatch;
        }
        value->data = member->getString().data();
        value->size = member->getString().size();
        return CrossaStatusOk;
    }

    // Reads one Bool value through a generated result path.
    CrossaStatus crossaGetPathBool(
        CrossaRuntimeHandle runtime,
        CrossaResultHandle result,
        const CrossaAbiPathSegment* path,
        size_t pathCount,
        uint8_t* value
    ) {
        if (value == nullptr) return CrossaStatusInvalidArgument;
        const auto root = crossa::bindings::sharedabi::CrossaAbiValueAccess::findResult(
            runtime, result
        );
        if (root == nullptr) return CrossaStatusInvalidHandle;
        const auto* member = crossa::bindings::sharedabi::CrossaAbiValueAccess::findPath(
            *root, path, pathCount
        );
        if (member == nullptr) return CrossaStatusOutOfBounds;
        if (member->getKind() != crossa::runtime::RuntimeValueKind::Bool) {
            return CrossaStatusTypeMismatch;
        }
        *value = member->getBool() ? 1 : 0;
        return CrossaStatusOk;
    }

}
