#include "crossa/bindings/android/AndroidJniBridge.h"

#include <memory>
#include <mutex>
#include <limits>
#include <string>
#include <utility>
#include <vector>

using namespace std;

namespace crossa::bindings::android {

    constexpr size_t MaximumJniArguments = 256U;
    constexpr size_t MaximumJniStringBytes = 8U * 1024U * 1024U;

    // Caches reusable JVM references and dispatches generated runtime operations.
    class AndroidJniMetadata final {
    public:
        // Initializes metadata once before the generated bridge is used.
        [[nodiscard]] static bool initialize(
            JavaVM* javaVm,
            JNIEnv* environment,
            const char* bridgeClassName,
            AndroidJniBridge::RuntimeCreator runtimeCreator
        ) noexcept;

        // Returns the generated runtime creator configured during JNI loading.
        [[nodiscard]] static AndroidJniBridge::RuntimeCreator runtimeCreator() noexcept;

        static JavaVM* javaVm_;
        static AndroidJniBridge::RuntimeCreator runtimeCreator_;
        static jclass argumentClass_;
        static jclass intArgumentClass_;
        static jclass longArgumentClass_;
        static jclass doubleArgumentClass_;
        static jclass stringArgumentClass_;
        static jclass boolArgumentClass_;
        static jclass callbackClass_;
        static jmethodID callbackComplete_;
        static jfieldID intValue_;
        static jfieldID longValue_;
        static jfieldID doubleValue_;
        static jfieldID stringValue_;
        static jfieldID boolValue_;
        static string packagePrefix_;
        static mutex mutex_;
    };

    JavaVM* AndroidJniMetadata::javaVm_ = nullptr;
    AndroidJniBridge::RuntimeCreator AndroidJniMetadata::runtimeCreator_ = nullptr;
    jclass AndroidJniMetadata::argumentClass_ = nullptr;
    jclass AndroidJniMetadata::intArgumentClass_ = nullptr;
    jclass AndroidJniMetadata::longArgumentClass_ = nullptr;
    jclass AndroidJniMetadata::doubleArgumentClass_ = nullptr;
    jclass AndroidJniMetadata::stringArgumentClass_ = nullptr;
    jclass AndroidJniMetadata::boolArgumentClass_ = nullptr;
    jclass AndroidJniMetadata::callbackClass_ = nullptr;
    jmethodID AndroidJniMetadata::callbackComplete_ = nullptr;
    jfieldID AndroidJniMetadata::intValue_ = nullptr;
    jfieldID AndroidJniMetadata::longValue_ = nullptr;
    jfieldID AndroidJniMetadata::doubleValue_ = nullptr;
    jfieldID AndroidJniMetadata::stringValue_ = nullptr;
    jfieldID AndroidJniMetadata::boolValue_ = nullptr;
    string AndroidJniMetadata::packagePrefix_;
    mutex AndroidJniMetadata::mutex_;

    // Holds a callback global reference until the ABI delivers one terminal state.
    class AndroidCompletionContext final {
    public:
        // Retains the callback independently from the originating JNI call.
        AndroidCompletionContext(JavaVM* javaVm, JNIEnv* environment, jobject callback)
            : javaVm_(javaVm), callback_(environment->NewGlobalRef(callback)) {}

        // Releases the callback global reference through an attached JNI environment.
        ~AndroidCompletionContext() {
            if (callback_ == nullptr || javaVm_ == nullptr) return;
            JNIEnv* environment = nullptr;
            bool attached = false;
            if (javaVm_->GetEnv(reinterpret_cast<void**>(&environment), JNI_VERSION_1_6) != JNI_OK) {
                if (javaVm_->AttachCurrentThread(&environment, nullptr) != JNI_OK) return;
                attached = true;
            }
            environment->DeleteGlobalRef(callback_);
            if (attached) javaVm_->DetachCurrentThread();
        }

        // Delivers one terminal operation state on the scheduler thread safely.
        void complete(
            CrossaAbiCompletionKind kind,
            CrossaResultHandle result,
            CrossaErrorHandle error
        ) noexcept {
            JNIEnv* environment = nullptr;
            bool attached = false;
            if (javaVm_->GetEnv(reinterpret_cast<void**>(&environment), JNI_VERSION_1_6) != JNI_OK) {
                if (javaVm_->AttachCurrentThread(&environment, nullptr) != JNI_OK) return;
                attached = true;
            }
            environment->CallVoidMethod(
                callback_,
                AndroidJniMetadata::callbackComplete_,
                static_cast<jint>(kind),
                static_cast<jlong>(result),
                static_cast<jlong>(error)
            );
            if (environment->ExceptionCheck()) environment->ExceptionClear();
            environment->DeleteGlobalRef(callback_);
            callback_ = nullptr;
            if (attached) javaVm_->DetachCurrentThread();
        }

        // Returns whether the callback global reference was allocated.
        [[nodiscard]] bool isValid() const noexcept {
            return callback_ != nullptr;
        }

    private:
        JavaVM* javaVm_;
        jobject callback_;
    };

    // Resolves and promotes a Kotlin class reference for persistent JNI use.
    static jclass findGlobalClass(JNIEnv* environment, const char* name) {
        jclass localClass = environment->FindClass(name);
        if (localClass == nullptr) return nullptr;
        jclass globalClass = static_cast<jclass>(environment->NewGlobalRef(localClass));
        environment->DeleteLocalRef(localClass);
        return globalClass;
    }

    // Converts the generated Kotlin scalar argument wrappers into ABI arguments.
    static bool convertArguments(
        JNIEnv* environment,
        jobjectArray arguments,
        vector<CrossaAbiArgument>* converted,
        vector<string>* strings
    ) {
        if (converted == nullptr || strings == nullptr) return false;
        const jsize size = arguments == nullptr ? 0 : environment->GetArrayLength(arguments);
        if (environment->ExceptionCheck() || size < 0 ||
            static_cast<size_t>(size) > MaximumJniArguments) return false;
        converted->clear();
        strings->clear();
        converted->reserve(static_cast<size_t>(size));
        strings->reserve(static_cast<size_t>(size));
        for (jsize index = 0; index < size; ++index) {
            jobject argument = environment->GetObjectArrayElement(arguments, index);
            if (argument == nullptr) return false;
            CrossaAbiArgument convertedArgument{};
            if (environment->IsInstanceOf(argument, AndroidJniMetadata::intArgumentClass_)) {
                convertedArgument.kind = CrossaAbiArgumentInt;
                convertedArgument.integerValue = environment->GetIntField(argument, AndroidJniMetadata::intValue_);
            } else if (environment->IsInstanceOf(argument, AndroidJniMetadata::longArgumentClass_)) {
                convertedArgument.kind = CrossaAbiArgumentLong;
                convertedArgument.integerValue = environment->GetLongField(argument, AndroidJniMetadata::longValue_);
            } else if (environment->IsInstanceOf(argument, AndroidJniMetadata::doubleArgumentClass_)) {
                convertedArgument.kind = CrossaAbiArgumentDouble;
                convertedArgument.doubleValue = environment->GetDoubleField(argument, AndroidJniMetadata::doubleValue_);
            } else if (environment->IsInstanceOf(argument, AndroidJniMetadata::boolArgumentClass_)) {
                convertedArgument.kind = CrossaAbiArgumentBool;
                convertedArgument.integerValue = environment->GetBooleanField(argument, AndroidJniMetadata::boolValue_) == JNI_TRUE;
            } else if (environment->IsInstanceOf(argument, AndroidJniMetadata::stringArgumentClass_)) {
                convertedArgument.kind = CrossaAbiArgumentString;
                jstring value = static_cast<jstring>(environment->GetObjectField(argument, AndroidJniMetadata::stringValue_));
                if (value == nullptr) {
                    environment->DeleteLocalRef(argument);
                    return false;
                }
                const jsize stringLength = environment->GetStringUTFLength(value);
                if (environment->ExceptionCheck() || stringLength < 0 ||
                    static_cast<size_t>(stringLength) > MaximumJniStringBytes) {
                    environment->DeleteLocalRef(value);
                    environment->DeleteLocalRef(argument);
                    return false;
                }
                const char* utf8 = environment->GetStringUTFChars(value, nullptr);
                if (utf8 == nullptr) {
                    environment->DeleteLocalRef(value);
                    environment->DeleteLocalRef(argument);
                    return false;
                }
                strings->emplace_back(utf8);
                environment->ReleaseStringUTFChars(value, utf8);
                environment->DeleteLocalRef(value);
                convertedArgument.stringValue = {strings->back().data(), strings->back().size()};
            } else {
                environment->DeleteLocalRef(argument);
                return false;
            }
            converted->push_back(convertedArgument);
            environment->DeleteLocalRef(argument);
        }
        return !environment->ExceptionCheck();
    }

    // Creates one generated runtime through the project-specific factory seam.
    static jlong nativeConfigure(JNIEnv*, jobject) {
        try {
            const auto creator = AndroidJniMetadata::runtimeCreator();
            return creator == nullptr ? 0 : static_cast<jlong>(creator());
        } catch (...) {
            return 0;
        }
    }

    // Starts a generated Async operation through the shared C ABI.
    static jlong nativeInvokeAsync(JNIEnv* environment, jobject, jlong runtime, jlong operation, jobjectArray arguments) {
        try {
            vector<CrossaAbiArgument> converted;
            vector<string> strings;
            if (!convertArguments(environment, arguments, &converted, &strings)) return 0;
            CrossaOperationHandle invocation = 0;
            return crossaInvokeAsync(static_cast<CrossaRuntimeHandle>(runtime), static_cast<CrossaOperationId>(operation), converted.data(), converted.size(), &invocation) == CrossaStatusOk ? static_cast<jlong>(invocation) : 0;
        } catch (...) {
            return 0;
        }
    }

    // Bridges one ABI completion to a persistent Kotlin callback reference.
    static void complete(void* userData, CrossaAbiCompletionKind kind, CrossaResultHandle result, CrossaErrorHandle error) noexcept {
        unique_ptr<AndroidCompletionContext> context(static_cast<AndroidCompletionContext*>(userData));
        context->complete(kind, result, error);
    }

    // Starts a generated AsyncAfter operation through the shared C ABI.
    static jlong nativeInvokeAsyncAfter(JNIEnv* environment, jobject, jlong runtime, jlong operation, jobjectArray arguments, jobject callback) {
        AndroidCompletionContext* callbackContext = nullptr;
        try {
            vector<CrossaAbiArgument> converted;
            vector<string> strings;
            if (callback == nullptr || !convertArguments(environment, arguments, &converted, &strings)) return 0;
            auto context = make_unique<AndroidCompletionContext>(AndroidJniMetadata::javaVm_, environment, callback);
            if (!context->isValid()) return 0;
            callbackContext = context.release();
            CrossaOperationHandle invocation = 0;
            const CrossaStatus status = crossaInvokeAsyncAfter(static_cast<CrossaRuntimeHandle>(runtime), static_cast<CrossaOperationId>(operation), converted.data(), converted.size(), complete, callbackContext, &invocation);
            if (status != CrossaStatusOk) {
                delete callbackContext;
                callbackContext = nullptr;
                return 0;
            }
            callbackContext = nullptr;
            return static_cast<jlong>(invocation);
        } catch (...) {
            delete callbackContext;
            return 0;
        }
    }

    // Requests cancellation for a generated runtime operation.
    static jboolean nativeCancel(JNIEnv*, jobject, jlong runtime, jlong operation) {
        return crossaCancelOperation(static_cast<CrossaRuntimeHandle>(runtime), static_cast<CrossaOperationHandle>(operation)) == CrossaStatusOk ? JNI_TRUE : JNI_FALSE;
    }

    // Releases a generated runtime operation handle.
    static void nativeReleaseOperation(JNIEnv*, jobject, jlong runtime, jlong operation) {
        (void)crossaReleaseOperation(static_cast<CrossaRuntimeHandle>(runtime), static_cast<CrossaOperationHandle>(operation));
    }

    // Stops a generated runtime without invalidating retained result views.
    static void nativeShutdown(JNIEnv*, jobject, jlong runtime) {
        (void)crossaRuntimeShutdown(static_cast<CrossaRuntimeHandle>(runtime));
    }

    // Releases a generated runtime and its remaining native handles.
    static void nativeReleaseRuntime(JNIEnv*, jobject, jlong runtime) {
        crossaReleaseRuntime(static_cast<CrossaRuntimeHandle>(runtime));
    }

    // Releases a native result after Kotlin closes its owning view.
    static void nativeReleaseResult(JNIEnv*, jobject, jlong runtime, jlong result) {
        (void)crossaReleaseResult(static_cast<CrossaRuntimeHandle>(runtime), static_cast<CrossaResultHandle>(result));
    }

    // Reads the native list size without materializing a Kotlin collection.
    static jint nativeListSize(JNIEnv*, jobject, jlong runtime, jlong result) {
        size_t size = 0;
        return crossaGetListSize(static_cast<CrossaRuntimeHandle>(runtime), static_cast<CrossaResultHandle>(result), &size) == CrossaStatusOk && size <= static_cast<size_t>(numeric_limits<jint>::max()) ? static_cast<jint>(size) : -1;
    }

    // Resolves one borrowed model index from a native list result.
    static jlong nativeListModelHandle(JNIEnv*, jobject, jlong runtime, jlong result, jint index) {
        CrossaModelHandle model = 0;
        return index >= 0 && crossaGetListModel(static_cast<CrossaRuntimeHandle>(runtime), static_cast<CrossaResultHandle>(result), static_cast<size_t>(index), &model) == CrossaStatusOk ? static_cast<jlong>(model) : 0;
    }

    // Resolves the root native model view from one result.
    static jlong nativeRootModelHandle(JNIEnv*, jobject, jlong runtime, jlong result) {
        CrossaModelHandle model = 0;
        return crossaGetRootModel(static_cast<CrossaRuntimeHandle>(runtime), static_cast<CrossaResultHandle>(result), &model) == CrossaStatusOk ? static_cast<jlong>(model) : 0;
    }

    // Reads one scalar root result value through the ABI.
    static jint nativeResultInt(JNIEnv*, jobject, jlong runtime, jlong result) { int32_t value = 0; return crossaGetResultInt(static_cast<CrossaRuntimeHandle>(runtime), static_cast<CrossaResultHandle>(result), &value) == CrossaStatusOk ? value : 0; }
    static jlong nativeResultLong(JNIEnv*, jobject, jlong runtime, jlong result) { int64_t value = 0; return crossaGetResultLong(static_cast<CrossaRuntimeHandle>(runtime), static_cast<CrossaResultHandle>(result), &value) == CrossaStatusOk ? value : 0; }
    static jdouble nativeResultDouble(JNIEnv*, jobject, jlong runtime, jlong result) { double value = 0; return crossaGetResultDouble(static_cast<CrossaRuntimeHandle>(runtime), static_cast<CrossaResultHandle>(result), &value) == CrossaStatusOk ? value : 0; }
    static jboolean nativeResultBoolean(JNIEnv*, jobject, jlong runtime, jlong result) { uint8_t value = 0; return crossaGetResultBool(static_cast<CrossaRuntimeHandle>(runtime), static_cast<CrossaResultHandle>(result), &value) == CrossaStatusOk && value ? JNI_TRUE : JNI_FALSE; }

    // Copies one runtime-owned UTF-8 view only at the Kotlin boundary.
    static jstring newString(JNIEnv* environment, CrossaStringView value, CrossaStatus status) {
        try {
            if (status != CrossaStatusOk || value.data == nullptr) return environment->NewStringUTF("");
            return environment->NewStringUTF(string(value.data, value.size).c_str());
        } catch (...) {
            return environment->NewStringUTF("");
        }
    }

    // Reads a scalar string root result through the ABI.
    static jstring nativeResultString(JNIEnv* environment, jobject, jlong runtime, jlong result) {
        CrossaStringView value{};
        const CrossaStatus status = crossaGetResultString(static_cast<CrossaRuntimeHandle>(runtime), static_cast<CrossaResultHandle>(result), &value);
        return newString(environment, value, status);
    }

    // Copies one structured native error message for Kotlin state delivery.
    static jstring nativeErrorMessage(JNIEnv* environment, jobject, jlong runtime, jlong error) {
        CrossaStringView value{};
        const CrossaStatus status = crossaGetErrorMessage(static_cast<CrossaRuntimeHandle>(runtime), static_cast<CrossaErrorHandle>(error), &value);
        return newString(environment, value, status);
    }

    static jlong nativeErrorMetadata(JNIEnv*, jobject, jlong runtime, jlong error) {
        int32_t domain = 0;
        int32_t code = 0;
        uint8_t retryable = 0;
        const CrossaStatus status = crossaGetErrorMetadata(
            static_cast<CrossaRuntimeHandle>(runtime),
            static_cast<CrossaErrorHandle>(error),
            &domain,
            &code,
            &retryable
        );
        if (status != CrossaStatusOk) return 0;
        return static_cast<jlong>(
            (uint64_t{1} << 63) |
            (static_cast<uint64_t>(retryable != 0) << 62) |
            ((static_cast<uint64_t>(code) & 0x7fffffffU) << 32) |
            (static_cast<uint32_t>(domain))
        );
    }

    // Releases one terminal error after Kotlin has copied its message.
    static void nativeReleaseError(JNIEnv*, jobject, jlong runtime, jlong error) { (void)crossaReleaseError(static_cast<CrossaRuntimeHandle>(runtime), static_cast<CrossaErrorHandle>(error)); }

    // Reads native model scalar fields without reflection or object graph copies.
    static jint nativeModelInt(JNIEnv*, jobject, jlong runtime, jlong result, jlong model, jint field) { int32_t value = 0; return field >= 0 && crossaGetModelInt(static_cast<CrossaRuntimeHandle>(runtime), static_cast<CrossaResultHandle>(result), static_cast<CrossaModelHandle>(model), static_cast<uint32_t>(field), &value) == CrossaStatusOk ? value : 0; }
    static jlong nativeModelLong(JNIEnv*, jobject, jlong runtime, jlong result, jlong model, jint field) { int64_t value = 0; return field >= 0 && crossaGetModelLong(static_cast<CrossaRuntimeHandle>(runtime), static_cast<CrossaResultHandle>(result), static_cast<CrossaModelHandle>(model), static_cast<uint32_t>(field), &value) == CrossaStatusOk ? value : 0; }
    static jdouble nativeModelDouble(JNIEnv*, jobject, jlong runtime, jlong result, jlong model, jint field) { double value = 0; return field >= 0 && crossaGetModelDouble(static_cast<CrossaRuntimeHandle>(runtime), static_cast<CrossaResultHandle>(result), static_cast<CrossaModelHandle>(model), static_cast<uint32_t>(field), &value) == CrossaStatusOk ? value : 0; }
    static jboolean nativeModelBoolean(JNIEnv*, jobject, jlong runtime, jlong result, jlong model, jint field) { uint8_t value = 0; return field >= 0 && crossaGetModelBool(static_cast<CrossaRuntimeHandle>(runtime), static_cast<CrossaResultHandle>(result), static_cast<CrossaModelHandle>(model), static_cast<uint32_t>(field), &value) == CrossaStatusOk && value ? JNI_TRUE : JNI_FALSE; }
    static jstring nativeModelString(JNIEnv* environment, jobject, jlong runtime, jlong result, jlong model, jint field) {
        if (field < 0) return environment->NewStringUTF("");
        CrossaStringView value{};
        const CrossaStatus status = crossaGetModelString(static_cast<CrossaRuntimeHandle>(runtime), static_cast<CrossaResultHandle>(result), static_cast<CrossaModelHandle>(model), static_cast<uint32_t>(field), &value);
        return newString(environment, value, status);
    }

    bool AndroidJniMetadata::initialize(JavaVM* javaVm, JNIEnv* environment, const char* bridgeClassName, AndroidJniBridge::RuntimeCreator runtimeCreator) noexcept {
        try {
            lock_guard lock(mutex_);
            if (javaVm_ != nullptr) return javaVm_ == javaVm && runtimeCreator_ == runtimeCreator;
            jclass bridgeClass = findGlobalClass(environment, bridgeClassName);
            packagePrefix_ = bridgeClassName;
            const size_t bridgeStart = packagePrefix_.rfind('/');
            if (bridgeStart == string::npos) return false;
            packagePrefix_.erase(bridgeStart + 1);
            argumentClass_ = findGlobalClass(environment, (packagePrefix_ + "CrossaArgument").c_str());
            intArgumentClass_ = findGlobalClass(environment, (packagePrefix_ + "CrossaArgument$IntValue").c_str());
            longArgumentClass_ = findGlobalClass(environment, (packagePrefix_ + "CrossaArgument$LongValue").c_str());
            doubleArgumentClass_ = findGlobalClass(environment, (packagePrefix_ + "CrossaArgument$DoubleValue").c_str());
            stringArgumentClass_ = findGlobalClass(environment, (packagePrefix_ + "CrossaArgument$StringValue").c_str());
            boolArgumentClass_ = findGlobalClass(environment, (packagePrefix_ + "CrossaArgument$BooleanValue").c_str());
            callbackClass_ = findGlobalClass(environment, (packagePrefix_ + "CrossaNativeCallback").c_str());
            if (bridgeClass == nullptr || argumentClass_ == nullptr || intArgumentClass_ == nullptr || longArgumentClass_ == nullptr || doubleArgumentClass_ == nullptr || stringArgumentClass_ == nullptr || boolArgumentClass_ == nullptr || callbackClass_ == nullptr) return false;
            intValue_ = environment->GetFieldID(intArgumentClass_, "value", "I");
            longValue_ = environment->GetFieldID(longArgumentClass_, "value", "J");
            doubleValue_ = environment->GetFieldID(doubleArgumentClass_, "value", "D");
            stringValue_ = environment->GetFieldID(stringArgumentClass_, "value", "Ljava/lang/String;");
            boolValue_ = environment->GetFieldID(boolArgumentClass_, "value", "Z");
            callbackComplete_ = environment->GetMethodID(callbackClass_, "onComplete", "(IJJ)V");
            if (intValue_ == nullptr || longValue_ == nullptr || doubleValue_ == nullptr || stringValue_ == nullptr || boolValue_ == nullptr || callbackComplete_ == nullptr) return false;
            const string argumentDescriptor = "[L" + packagePrefix_ + "CrossaArgument;";
            const string callbackDescriptor = "L" + packagePrefix_ + "CrossaNativeCallback;";
            const string invokeDescriptor = "(JJ" + argumentDescriptor + ")J";
            const string completionDescriptor = "(JJ" + argumentDescriptor + callbackDescriptor + ")J";
            JNINativeMethod methods[] = {
            {const_cast<char*>("nativeConfigure"), const_cast<char*>("()J"), reinterpret_cast<void*>(nativeConfigure)},
            {const_cast<char*>("nativeInvokeAsync"), const_cast<char*>(invokeDescriptor.c_str()), reinterpret_cast<void*>(nativeInvokeAsync)},
            {const_cast<char*>("nativeInvokeAsyncAfter"), const_cast<char*>(completionDescriptor.c_str()), reinterpret_cast<void*>(nativeInvokeAsyncAfter)},
            {const_cast<char*>("nativeCancel"), const_cast<char*>("(JJ)Z"), reinterpret_cast<void*>(nativeCancel)},
            {const_cast<char*>("nativeReleaseOperation"), const_cast<char*>("(JJ)V"), reinterpret_cast<void*>(nativeReleaseOperation)},
            {const_cast<char*>("nativeShutdown"), const_cast<char*>("(J)V"), reinterpret_cast<void*>(nativeShutdown)},
            {const_cast<char*>("nativeReleaseRuntime"), const_cast<char*>("(J)V"), reinterpret_cast<void*>(nativeReleaseRuntime)},
            {const_cast<char*>("nativeReleaseResult"), const_cast<char*>("(JJ)V"), reinterpret_cast<void*>(nativeReleaseResult)},
            {const_cast<char*>("nativeListSize"), const_cast<char*>("(JJ)I"), reinterpret_cast<void*>(nativeListSize)},
            {const_cast<char*>("nativeListModelHandle"), const_cast<char*>("(JJI)J"), reinterpret_cast<void*>(nativeListModelHandle)},
            {const_cast<char*>("nativeRootModelHandle"), const_cast<char*>("(JJ)J"), reinterpret_cast<void*>(nativeRootModelHandle)},
            {const_cast<char*>("nativeResultInt"), const_cast<char*>("(JJ)I"), reinterpret_cast<void*>(nativeResultInt)},
            {const_cast<char*>("nativeResultLong"), const_cast<char*>("(JJ)J"), reinterpret_cast<void*>(nativeResultLong)},
            {const_cast<char*>("nativeResultDouble"), const_cast<char*>("(JJ)D"), reinterpret_cast<void*>(nativeResultDouble)},
            {const_cast<char*>("nativeResultString"), const_cast<char*>("(JJ)Ljava/lang/String;"), reinterpret_cast<void*>(nativeResultString)},
            {const_cast<char*>("nativeErrorMessage"), const_cast<char*>("(JJ)Ljava/lang/String;"), reinterpret_cast<void*>(nativeErrorMessage)},
            {const_cast<char*>("nativeErrorMetadata"), const_cast<char*>("(JJ)J"), reinterpret_cast<void*>(nativeErrorMetadata)},
            {const_cast<char*>("nativeReleaseError"), const_cast<char*>("(JJ)V"), reinterpret_cast<void*>(nativeReleaseError)},
            {const_cast<char*>("nativeResultBoolean"), const_cast<char*>("(JJ)Z"), reinterpret_cast<void*>(nativeResultBoolean)},
            {const_cast<char*>("nativeModelInt"), const_cast<char*>("(JJJI)I"), reinterpret_cast<void*>(nativeModelInt)},
            {const_cast<char*>("nativeModelLong"), const_cast<char*>("(JJJI)J"), reinterpret_cast<void*>(nativeModelLong)},
            {const_cast<char*>("nativeModelDouble"), const_cast<char*>("(JJJI)D"), reinterpret_cast<void*>(nativeModelDouble)},
            {const_cast<char*>("nativeModelString"), const_cast<char*>("(JJJI)Ljava/lang/String;"), reinterpret_cast<void*>(nativeModelString)},
            {const_cast<char*>("nativeModelBoolean"), const_cast<char*>("(JJJI)Z"), reinterpret_cast<void*>(nativeModelBoolean)}
            };
            if (environment->RegisterNatives(bridgeClass, methods, sizeof(methods) / sizeof(methods[0])) != JNI_OK) return false;
            environment->DeleteGlobalRef(bridgeClass);
            javaVm_ = javaVm;
            runtimeCreator_ = runtimeCreator;
            return true;
        } catch (...) {
            return false;
        }
    }

    AndroidJniBridge::RuntimeCreator AndroidJniMetadata::runtimeCreator() noexcept {
        try {
            lock_guard lock(mutex_);
            return runtimeCreator_;
        } catch (...) {
            return nullptr;
        }
    }

    bool AndroidJniBridge::initialize(JavaVM* javaVm, JNIEnv* environment, const char* bridgeClassName, RuntimeCreator runtimeCreator) noexcept {
        return AndroidJniMetadata::initialize(javaVm, environment, bridgeClassName, runtimeCreator);
    }

}
