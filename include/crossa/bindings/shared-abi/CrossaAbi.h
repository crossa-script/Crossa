#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Defines the durable C boundary consumed by Android and iOS bindings.
#define CROSSA_ABI_VERSION 1U

// Identifies one opaque runtime-owned handle.
typedef uint64_t CrossaRuntimeHandle;

// Identifies one opaque runtime-owned native result.
typedef uint64_t CrossaResultHandle;

// Identifies one borrowed native model view inside a result.
typedef uint64_t CrossaModelHandle;

// Identifies stable success and failure outcomes from ABI calls.
typedef enum CrossaStatus {
    CrossaStatusOk = 0,
    CrossaStatusInvalidHandle = 1,
    CrossaStatusInvalidArgument = 2,
    CrossaStatusTypeMismatch = 3,
    CrossaStatusOutOfBounds = 4,
    CrossaStatusInternalError = 5
} CrossaStatus;

// Distinguishes the native result shape exposed through a handle.
typedef enum CrossaValueKind {
    CrossaValueUnit = 0,
    CrossaValueInt = 1,
    CrossaValueLong = 2,
    CrossaValueDouble = 3,
    CrossaValueString = 4,
    CrossaValueBool = 5,
    CrossaValueModel = 6,
    CrossaValueList = 7
} CrossaValueKind;

// Views UTF-8 result text until the associated result is released.
typedef struct CrossaStringView {
    const char* data;
    size_t size;
} CrossaStringView;

// Creates an empty runtime context that owns ABI result handles.
CrossaStatus crossaCreateRuntime(CrossaRuntimeHandle* runtime);

// Destroys a runtime context and invalidates all its result handles.
void crossaReleaseRuntime(CrossaRuntimeHandle runtime);

// Releases a result exactly once; repeated calls fail safely.
CrossaStatus crossaReleaseResult(
    CrossaRuntimeHandle runtime,
    CrossaResultHandle result
);

// Returns the root result value category.
CrossaStatus crossaGetResultKind(
    CrossaRuntimeHandle runtime,
    CrossaResultHandle result,
    CrossaValueKind* kind
);

// Returns the number of elements in a native list result.
CrossaStatus crossaGetListSize(
    CrossaRuntimeHandle runtime,
    CrossaResultHandle result,
    size_t* size
);

// Returns a borrowed model handle for one list element.
CrossaStatus crossaGetListModel(
    CrossaRuntimeHandle runtime,
    CrossaResultHandle result,
    size_t index,
    CrossaModelHandle* model
);

// Returns the root model handle when a result is a native model.
CrossaStatus crossaGetRootModel(
    CrossaRuntimeHandle runtime,
    CrossaResultHandle result,
    CrossaModelHandle* model
);

// Reads one Int field using a generated declaration-order field identifier.
CrossaStatus crossaGetModelInt(
    CrossaRuntimeHandle runtime,
    CrossaResultHandle result,
    CrossaModelHandle model,
    uint32_t field,
    int32_t* value
);

// Reads one Long field using a generated declaration-order field identifier.
CrossaStatus crossaGetModelLong(
    CrossaRuntimeHandle runtime,
    CrossaResultHandle result,
    CrossaModelHandle model,
    uint32_t field,
    int64_t* value
);

// Reads one Double field using a generated declaration-order field identifier.
CrossaStatus crossaGetModelDouble(
    CrossaRuntimeHandle runtime,
    CrossaResultHandle result,
    CrossaModelHandle model,
    uint32_t field,
    double* value
);

// Reads one String field using a generated declaration-order field identifier.
CrossaStatus crossaGetModelString(
    CrossaRuntimeHandle runtime,
    CrossaResultHandle result,
    CrossaModelHandle model,
    uint32_t field,
    CrossaStringView* value
);

// Reads one Bool field using a generated declaration-order field identifier.
CrossaStatus crossaGetModelBool(
    CrossaRuntimeHandle runtime,
    CrossaResultHandle result,
    CrossaModelHandle model,
    uint32_t field,
    uint8_t* value
);

#ifdef __cplusplus
}
#endif
