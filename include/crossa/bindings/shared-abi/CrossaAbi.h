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

// Identifies one opaque runtime-owned native error.
typedef uint64_t CrossaErrorHandle;

// Identifies one accepted native asynchronous invocation.
typedef uint64_t CrossaOperationHandle;

// Identifies one deterministic generated runtime operation.
typedef uint64_t CrossaOperationId;

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
    CrossaValueList = 7,
    CrossaValueJson = 8
} CrossaValueKind;

// Distinguishes one explicit native Json value without exposing the DOM layout.
typedef enum CrossaJsonKind {
    CrossaJsonNull = 0,
    CrossaJsonBoolean = 1,
    CrossaJsonNumber = 2,
    CrossaJsonString = 3,
    CrossaJsonArray = 4,
    CrossaJsonObject = 5
} CrossaJsonKind;

// Views UTF-8 result text until the associated result is released.
typedef struct CrossaStringView {
    const char* data;
    size_t size;
} CrossaStringView;

// Identifies one immutable navigation step inside a result-owned value tree.
typedef enum CrossaAbiPathSegmentKind {
    CrossaAbiPathField = 0,
    CrossaAbiPathListElement = 1
} CrossaAbiPathSegmentKind;

// Carries one generated field or list index without exposing native objects.
typedef struct CrossaAbiPathSegment {
    CrossaAbiPathSegmentKind kind;
    uint32_t index;
} CrossaAbiPathSegment;

// Distinguishes the POD argument values accepted by native invocation.
typedef enum CrossaAbiArgumentKind {
    CrossaAbiArgumentInt = 0,
    CrossaAbiArgumentLong = 1,
    CrossaAbiArgumentDouble = 2,
    CrossaAbiArgumentBool = 3,
    CrossaAbiArgumentString = 4
} CrossaAbiArgumentKind;

// Carries one non-owning typed argument for the duration of an ABI call.
typedef struct CrossaAbiArgument {
    CrossaAbiArgumentKind kind;
    int64_t integerValue;
    double doubleValue;
    CrossaStringView stringValue;
} CrossaAbiArgument;

// Identifies the terminal state delivered for an asynchronous operation.
typedef enum CrossaAbiCompletionKind {
    CrossaAbiCompletionSuccess = 0,
    CrossaAbiCompletionFailed = 1,
    CrossaAbiCompletionCancelled = 2
} CrossaAbiCompletionKind;

// Delivers one terminal operation state; result and error belong to runtime.
typedef void (*CrossaAbiCompletion)(
    void* userData,
    CrossaAbiCompletionKind kind,
    CrossaResultHandle result,
    CrossaErrorHandle error
);

// Starts a generated fire-and-forget Async operation.
CrossaStatus crossaInvokeAsync(
    CrossaRuntimeHandle runtime,
    CrossaOperationId operation,
    const CrossaAbiArgument* arguments,
    size_t argumentCount,
    CrossaOperationHandle* invocation
);

// Starts a generated AsyncAfter operation with exactly one terminal callback.
CrossaStatus crossaInvokeAsyncAfter(
    CrossaRuntimeHandle runtime,
    CrossaOperationId operation,
    const CrossaAbiArgument* arguments,
    size_t argumentCount,
    CrossaAbiCompletion completion,
    void* userData,
    CrossaOperationHandle* invocation
);

// Requests idempotent cancellation for one accepted native operation.
CrossaStatus crossaCancelOperation(
    CrossaRuntimeHandle runtime,
    CrossaOperationHandle operation
);

// Releases the caller-owned handle for one terminal native operation.
CrossaStatus crossaReleaseOperation(
    CrossaRuntimeHandle runtime,
    CrossaOperationHandle operation
);

// Deprecated: runtime construction requires a generated native program.
CrossaStatus crossaCreateRuntime(CrossaRuntimeHandle* runtime);

// Destroys a runtime and invalidates every remaining result and error handle.
void crossaReleaseRuntime(CrossaRuntimeHandle runtime);

// Stops a runtime, cancels accepted operations, and preserves retained results.
CrossaStatus crossaRuntimeShutdown(CrossaRuntimeHandle runtime);

// Releases a result exactly once; repeated calls fail safely.
CrossaStatus crossaReleaseResult(
    CrossaRuntimeHandle runtime,
    CrossaResultHandle result
);

// Releases one error and makes future accesses fail deterministically.
CrossaStatus crossaReleaseError(
    CrossaRuntimeHandle runtime,
    CrossaErrorHandle error
);

// Reads a borrowed error message valid until the error handle is released.
CrossaStatus crossaGetErrorMessage(
    CrossaRuntimeHandle runtime,
    CrossaErrorHandle error,
    CrossaStringView* value
);

// Reads stable numeric error metadata without exposing C++ error objects.
CrossaStatus crossaGetErrorMetadata(
    CrossaRuntimeHandle runtime,
    CrossaErrorHandle error,
    int32_t* domain,
    int32_t* code,
    uint8_t* retryable
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

// Reads an Int root result.
CrossaStatus crossaGetResultInt(
    CrossaRuntimeHandle runtime,
    CrossaResultHandle result,
    int32_t* value
);

// Reads a Long root result.
CrossaStatus crossaGetResultLong(
    CrossaRuntimeHandle runtime,
    CrossaResultHandle result,
    int64_t* value
);

// Reads a Double root result.
CrossaStatus crossaGetResultDouble(
    CrossaRuntimeHandle runtime,
    CrossaResultHandle result,
    double* value
);

// Reads a String root result.
CrossaStatus crossaGetResultString(
    CrossaRuntimeHandle runtime,
    CrossaResultHandle result,
    CrossaStringView* value
);

// Reads a Bool root result.
CrossaStatus crossaGetResultBool(
    CrossaRuntimeHandle runtime,
    CrossaResultHandle result,
    uint8_t* value
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

// Returns the value category reached through a generated result path.
CrossaStatus crossaGetPathKind(
    CrossaRuntimeHandle runtime,
    CrossaResultHandle result,
    const CrossaAbiPathSegment* path,
    size_t pathCount,
    CrossaValueKind* kind
);

// Returns the number of elements in a list reached through a result path.
CrossaStatus crossaGetPathListSize(
    CrossaRuntimeHandle runtime,
    CrossaResultHandle result,
    const CrossaAbiPathSegment* path,
    size_t pathCount,
    size_t* size
);

// Reads an Int value reached through a generated result path.
CrossaStatus crossaGetPathInt(
    CrossaRuntimeHandle runtime,
    CrossaResultHandle result,
    const CrossaAbiPathSegment* path,
    size_t pathCount,
    int32_t* value
);

// Reads a Long value reached through a generated result path.
CrossaStatus crossaGetPathLong(
    CrossaRuntimeHandle runtime,
    CrossaResultHandle result,
    const CrossaAbiPathSegment* path,
    size_t pathCount,
    int64_t* value
);

// Reads a Double value reached through a generated result path.
CrossaStatus crossaGetPathDouble(
    CrossaRuntimeHandle runtime,
    CrossaResultHandle result,
    const CrossaAbiPathSegment* path,
    size_t pathCount,
    double* value
);

// Reads a String view reached through a generated result path.
CrossaStatus crossaGetPathString(
    CrossaRuntimeHandle runtime,
    CrossaResultHandle result,
    const CrossaAbiPathSegment* path,
    size_t pathCount,
    CrossaStringView* value
);

// Reads a Bool value reached through a generated result path.
CrossaStatus crossaGetPathBool(
    CrossaRuntimeHandle runtime,
    CrossaResultHandle result,
    const CrossaAbiPathSegment* path,
    size_t pathCount,
    uint8_t* value
);

// Returns the Json category reached through a generated result path.
CrossaStatus crossaGetPathJsonKind(
    CrossaRuntimeHandle runtime,
    CrossaResultHandle result,
    const CrossaAbiPathSegment* path,
    size_t pathCount,
    CrossaJsonKind* kind
);

// Reads a Json boolean reached through a generated result path.
CrossaStatus crossaGetPathJsonBool(
    CrossaRuntimeHandle runtime,
    CrossaResultHandle result,
    const CrossaAbiPathSegment* path,
    size_t pathCount,
    uint8_t* value
);

// Reads exact Json number text reached through a generated result path.
CrossaStatus crossaGetPathJsonNumber(
    CrossaRuntimeHandle runtime,
    CrossaResultHandle result,
    const CrossaAbiPathSegment* path,
    size_t pathCount,
    CrossaStringView* value
);

// Reads a Json string reached through a generated result path.
CrossaStatus crossaGetPathJsonString(
    CrossaRuntimeHandle runtime,
    CrossaResultHandle result,
    const CrossaAbiPathSegment* path,
    size_t pathCount,
    CrossaStringView* value
);

// Returns Json array length or object field count through a generated path.
CrossaStatus crossaGetPathJsonSize(
    CrossaRuntimeHandle runtime,
    CrossaResultHandle result,
    const CrossaAbiPathSegment* path,
    size_t pathCount,
    size_t* size
);

// Reads one Json object key in source order through a generated path.
CrossaStatus crossaGetPathJsonKey(
    CrossaRuntimeHandle runtime,
    CrossaResultHandle result,
    const CrossaAbiPathSegment* path,
    size_t pathCount,
    uint32_t field,
    CrossaStringView* value
);

#ifdef __cplusplus
}
#endif
