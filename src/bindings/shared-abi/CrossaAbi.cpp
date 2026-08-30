#include "crossa/bindings/shared-abi/CrossaAbi.h"

#include <memory>
#include <new>

#include "crossa/bindings/shared-abi/CrossaRuntimeContext.h"
#include "crossa/runtime/objects/NativeList.h"
#include "crossa/runtime/objects/NativeModel.h"

using namespace std;

namespace crossa::bindings::sharedabi {

    // Converts one ABI runtime handle into its native context when it is present.
    CrossaRuntimeContext* runtimeFromHandle(CrossaRuntimeHandle runtime) {
        return runtime == 0 ? nullptr : reinterpret_cast<CrossaRuntimeContext*>(runtime);
    }

    // Resolves one result handle while preserving its native storage lifetime.
    shared_ptr<const runtime::RuntimeValue> findResult(
        CrossaRuntimeHandle runtime,
        CrossaResultHandle result
    ) {
        CrossaRuntimeContext* context = runtimeFromHandle(runtime);
        return context == nullptr ? nullptr : context->findResult(result);
    }

    // Resolves one model view encoded as a root or list-element index.
    const runtime::NativeModel* findModel(
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
        return value.getKind() == runtime::RuntimeValueKind::Model ? &value.getModel() : nullptr;
    }

    // Resolves one schema-order field from a native model view.
    const runtime::RuntimeValue* findField(
        const runtime::RuntimeValue& result,
        CrossaModelHandle model,
        uint32_t field
    ) {
        const runtime::NativeModel* nativeModel = findModel(result, model);
        if (nativeModel == nullptr || field >= nativeModel->getFields().size()) return nullptr;
        return &nativeModel->getFields()[field].second;
    }

    // Maps one native runtime value type to its stable ABI category.
    CrossaValueKind mapKind(runtime::RuntimeValueKind kind) {
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

}

extern "C" {

    // Creates a runtime context that owns all results returned through this ABI.
    CrossaStatus crossaCreateRuntime(CrossaRuntimeHandle* runtime) {
        if (runtime == nullptr) return CrossaStatusInvalidArgument;
        try {
            *runtime = reinterpret_cast<CrossaRuntimeHandle>(new crossa::bindings::sharedabi::CrossaRuntimeContext());
            return CrossaStatusOk;
        } catch (const bad_alloc&) {
            *runtime = 0;
            return CrossaStatusInternalError;
        } catch (...) {
            *runtime = 0;
            return CrossaStatusInternalError;
        }
    }

    // Destroys a runtime context after invalidating all results it owns.
    void crossaReleaseRuntime(CrossaRuntimeHandle runtime) {
        delete crossa::bindings::sharedabi::runtimeFromHandle(runtime);
    }

    // Releases one result and makes future accesses fail deterministically.
    CrossaStatus crossaReleaseResult(CrossaRuntimeHandle runtime, CrossaResultHandle result) {
        crossa::bindings::sharedabi::CrossaRuntimeContext* context = crossa::bindings::sharedabi::runtimeFromHandle(runtime);
        return context == nullptr ? CrossaStatusInvalidHandle : context->releaseResult(result);
    }

    // Reads a result category without exposing native implementation types.
    CrossaStatus crossaGetResultKind(CrossaRuntimeHandle runtime, CrossaResultHandle result, CrossaValueKind* kind) {
        if (kind == nullptr) return CrossaStatusInvalidArgument;
        const auto value = crossa::bindings::sharedabi::findResult(runtime, result);
        if (value == nullptr) return CrossaStatusInvalidHandle;
        *kind = crossa::bindings::sharedabi::mapKind(value->getKind());
        return CrossaStatusOk;
    }

    // Returns the element count for a native list result.
    CrossaStatus crossaGetListSize(CrossaRuntimeHandle runtime, CrossaResultHandle result, size_t* size) {
        if (size == nullptr) return CrossaStatusInvalidArgument;
        const auto value = crossa::bindings::sharedabi::findResult(runtime, result);
        if (value == nullptr) return CrossaStatusInvalidHandle;
        if (value->getKind() != crossa::runtime::RuntimeValueKind::List) return CrossaStatusTypeMismatch;
        *size = value->getList().getSize();
        return CrossaStatusOk;
    }

    // Returns a borrowed list-model view encoded relative to the retained result.
    CrossaStatus crossaGetListModel(CrossaRuntimeHandle runtime, CrossaResultHandle result, size_t index, CrossaModelHandle* model) {
        if (model == nullptr) return CrossaStatusInvalidArgument;
        const auto value = crossa::bindings::sharedabi::findResult(runtime, result);
        if (value == nullptr) return CrossaStatusInvalidHandle;
        if (value->getKind() != crossa::runtime::RuntimeValueKind::List) return CrossaStatusTypeMismatch;
        if (index >= value->getList().getSize() || value->getList().get(index).getKind() != crossa::runtime::RuntimeValueKind::Model) return CrossaStatusOutOfBounds;
        *model = static_cast<CrossaModelHandle>(index + 1);
        return CrossaStatusOk;
    }

    // Returns the root model view for a retained model result.
    CrossaStatus crossaGetRootModel(CrossaRuntimeHandle runtime, CrossaResultHandle result, CrossaModelHandle* model) {
        if (model == nullptr) return CrossaStatusInvalidArgument;
        const auto value = crossa::bindings::sharedabi::findResult(runtime, result);
        if (value == nullptr) return CrossaStatusInvalidHandle;
        if (value->getKind() != crossa::runtime::RuntimeValueKind::Model) return CrossaStatusTypeMismatch;
        *model = 1;
        return CrossaStatusOk;
    }

    // Reads one Int root result through its retained opaque handle.
    CrossaStatus crossaGetResultInt(CrossaRuntimeHandle runtime, CrossaResultHandle result, int32_t* value) {
        if (value == nullptr) return CrossaStatusInvalidArgument;
        const auto root = crossa::bindings::sharedabi::findResult(runtime, result);
        if (root == nullptr) return CrossaStatusInvalidHandle;
        if (root->getKind() != crossa::runtime::RuntimeValueKind::Int) return CrossaStatusTypeMismatch;
        *value = static_cast<int32_t>(root->getInt());
        return CrossaStatusOk;
    }

    // Reads one Long root result through its retained opaque handle.
    CrossaStatus crossaGetResultLong(CrossaRuntimeHandle runtime, CrossaResultHandle result, int64_t* value) {
        if (value == nullptr) return CrossaStatusInvalidArgument;
        const auto root = crossa::bindings::sharedabi::findResult(runtime, result);
        if (root == nullptr) return CrossaStatusInvalidHandle;
        if (root->getKind() != crossa::runtime::RuntimeValueKind::Long) return CrossaStatusTypeMismatch;
        *value = root->getLong();
        return CrossaStatusOk;
    }

    // Reads one Double root result through its retained opaque handle.
    CrossaStatus crossaGetResultDouble(CrossaRuntimeHandle runtime, CrossaResultHandle result, double* value) {
        if (value == nullptr) return CrossaStatusInvalidArgument;
        const auto root = crossa::bindings::sharedabi::findResult(runtime, result);
        if (root == nullptr) return CrossaStatusInvalidHandle;
        if (root->getKind() != crossa::runtime::RuntimeValueKind::Double) return CrossaStatusTypeMismatch;
        *value = root->getDouble();
        return CrossaStatusOk;
    }

    // Reads one String root result as a view valid until result release.
    CrossaStatus crossaGetResultString(CrossaRuntimeHandle runtime, CrossaResultHandle result, CrossaStringView* value) {
        if (value == nullptr) return CrossaStatusInvalidArgument;
        const auto root = crossa::bindings::sharedabi::findResult(runtime, result);
        if (root == nullptr) return CrossaStatusInvalidHandle;
        if (root->getKind() != crossa::runtime::RuntimeValueKind::String) return CrossaStatusTypeMismatch;
        value->data = root->getString().data();
        value->size = root->getString().size();
        return CrossaStatusOk;
    }

    // Reads one Bool root result through its retained opaque handle.
    CrossaStatus crossaGetResultBool(CrossaRuntimeHandle runtime, CrossaResultHandle result, uint8_t* value) {
        if (value == nullptr) return CrossaStatusInvalidArgument;
        const auto root = crossa::bindings::sharedabi::findResult(runtime, result);
        if (root == nullptr) return CrossaStatusInvalidHandle;
        if (root->getKind() != crossa::runtime::RuntimeValueKind::Bool) return CrossaStatusTypeMismatch;
        *value = root->getBool() ? 1 : 0;
        return CrossaStatusOk;
    }

    // Reads one Int model field by stable generated index.
    CrossaStatus crossaGetModelInt(CrossaRuntimeHandle runtime, CrossaResultHandle result, CrossaModelHandle model, uint32_t field, int32_t* value) {
        if (value == nullptr) return CrossaStatusInvalidArgument;
        const auto root = crossa::bindings::sharedabi::findResult(runtime, result);
        const auto* member = root == nullptr ? nullptr : crossa::bindings::sharedabi::findField(*root, model, field);
        if (root == nullptr) return CrossaStatusInvalidHandle;
        if (member == nullptr) return CrossaStatusOutOfBounds;
        if (member->getKind() != crossa::runtime::RuntimeValueKind::Int) return CrossaStatusTypeMismatch;
        *value = static_cast<int32_t>(member->getInt());
        return CrossaStatusOk;
    }

    // Reads one Long model field by stable generated index.
    CrossaStatus crossaGetModelLong(CrossaRuntimeHandle runtime, CrossaResultHandle result, CrossaModelHandle model, uint32_t field, int64_t* value) {
        if (value == nullptr) return CrossaStatusInvalidArgument;
        const auto root = crossa::bindings::sharedabi::findResult(runtime, result);
        const auto* member = root == nullptr ? nullptr : crossa::bindings::sharedabi::findField(*root, model, field);
        if (root == nullptr) return CrossaStatusInvalidHandle;
        if (member == nullptr) return CrossaStatusOutOfBounds;
        if (member->getKind() != crossa::runtime::RuntimeValueKind::Long) return CrossaStatusTypeMismatch;
        *value = member->getLong();
        return CrossaStatusOk;
    }

    // Reads one Double model field by stable generated index.
    CrossaStatus crossaGetModelDouble(CrossaRuntimeHandle runtime, CrossaResultHandle result, CrossaModelHandle model, uint32_t field, double* value) {
        if (value == nullptr) return CrossaStatusInvalidArgument;
        const auto root = crossa::bindings::sharedabi::findResult(runtime, result);
        const auto* member = root == nullptr ? nullptr : crossa::bindings::sharedabi::findField(*root, model, field);
        if (root == nullptr) return CrossaStatusInvalidHandle;
        if (member == nullptr) return CrossaStatusOutOfBounds;
        if (member->getKind() != crossa::runtime::RuntimeValueKind::Double) return CrossaStatusTypeMismatch;
        *value = member->getDouble();
        return CrossaStatusOk;
    }

    // Reads one String model field by stable generated index.
    CrossaStatus crossaGetModelString(CrossaRuntimeHandle runtime, CrossaResultHandle result, CrossaModelHandle model, uint32_t field, CrossaStringView* value) {
        if (value == nullptr) return CrossaStatusInvalidArgument;
        const auto root = crossa::bindings::sharedabi::findResult(runtime, result);
        const auto* member = root == nullptr ? nullptr : crossa::bindings::sharedabi::findField(*root, model, field);
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
        const auto root = crossa::bindings::sharedabi::findResult(runtime, result);
        const auto* member = root == nullptr ? nullptr : crossa::bindings::sharedabi::findField(*root, model, field);
        if (root == nullptr) return CrossaStatusInvalidHandle;
        if (member == nullptr) return CrossaStatusOutOfBounds;
        if (member->getKind() != crossa::runtime::RuntimeValueKind::Bool) return CrossaStatusTypeMismatch;
        *value = member->getBool() ? 1 : 0;
        return CrossaStatusOk;
    }

}
