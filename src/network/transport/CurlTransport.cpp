#include "crossa/network/transport/CurlTransport.h"

#include <array>
#include <chrono>
#include <condition_variable>
#include <curl/curl.h>
#include <limits>
#include <mutex>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#if defined(CROSSA_ANDROID_EMBEDDED_CA_BUNDLE)
#include "CrossaAndroidCaBundle.h"
#endif

#include "crossa/network/HttpHeader.h"
#include "crossa/network/HttpMethod.h"
#include "crossa/network/NetworkPolicy.h"
#include "crossa/runtime/errors/CrossaException.h"

using namespace std;

namespace crossa::network::transport {

// Implements handle pooling and libcurl callbacks behind the public boundary.
class CurlTransport::Implementation final {
public:
    // Initializes process curl state and allocates reusable easy handles.
    explicit Implementation(size_t poolSize)
        : stopping_(false) {
        if (poolSize == 0) {
            throw invalid_argument("Curl transport pool size must be positive.");
        }
        if (curl_global_init(CURL_GLOBAL_DEFAULT) != CURLE_OK) {
            throw runtime_error("Unable to initialize the native HTTP runtime.");
        }
        try {
            handles_.reserve(poolSize);
            availableHandles_.reserve(poolSize);
            for (size_t index = 0; index < poolSize; ++index) {
                CURL* handle = curl_easy_init();
                if (handle == nullptr) {
                    throw runtime_error(
                        "Unable to create a native HTTP connection handle."
                    );
                }
                handles_.push_back(handle);
                availableHandles_.push_back(handle);
            }
        } catch (...) {
            releaseHandles();
            curl_global_cleanup();
            throw;
        }
    }

    // Stops acquisitions and releases every native easy handle.
    ~Implementation() {
        {
            unique_lock lock(mutex_);
            stopping_ = true;
            for (const auto& [handle, requestHandle] : activeRequests_) {
                (void)handle;
                (void)requestHandle.cancel();
            }
            available_.notify_all();
            drained_.wait(lock, [this]() {
                return activeRequests_.empty();
            });
        }
        releaseHandles();
        curl_global_cleanup();
    }

    // Performs one request using an acquired reusable easy handle.
    response::HttpResponse execute(
        const request::PreparedRequest& request,
        const runtime::RequestHandle& requestHandle
    ) {
        requestHandle.throwIfCancellationRequested();
        CURL* handle = acquireHandle(requestHandle);
        try {
            response::HttpResponse response = perform(
                handle,
                request,
                requestHandle
            );
            releaseHandle(handle);
            return response;
        } catch (...) {
            releaseHandle(handle);
            throw;
        }
    }

private:
    // Stores one bounded response body and its transport overflow state.
    struct ResponseBuffer final {
        string body;
        size_t maximumBytes;
        bool overflowed;
        response::TransferMetrics metrics;
    };

    struct ResponseHeaderBuffer final {
        vector<HttpHeader> headers;
        size_t maximumBytes;
        size_t maximumCount;
        size_t receivedBytes;
        bool overflowed;
    };

    struct ProgressContext final {
        const runtime::RequestHandle* requestHandle;
        response::TransferMetrics* metrics;
    };

    // Stops libcurl when the shared native handle requests cancellation.
    static int observeProgress(
        void* context,
        curl_off_t downloadTotal,
        curl_off_t downloadCurrent,
        curl_off_t uploadTotal,
        curl_off_t uploadCurrent
    ) noexcept {
        (void)downloadTotal;
        (void)downloadCurrent;
        (void)uploadTotal;
        (void)uploadCurrent;
        auto& progress = *static_cast<ProgressContext*>(context);
        progress.metrics->uploadTotal = static_cast<uint64_t>(uploadTotal);
        progress.metrics->uploadBytes = static_cast<uint64_t>(uploadCurrent);
        progress.metrics->downloadTotal = static_cast<uint64_t>(downloadTotal);
        progress.metrics->downloadBytes = static_cast<uint64_t>(downloadCurrent);
        ++progress.metrics->progressEvents;
        return progress.requestHandle->isCancellationRequested() ? 1 : 0;
    }

    // Appends response bytes while enforcing the configured size limit.
    static size_t writeBody(
        char* data,
        size_t elementSize,
        size_t elementCount,
        void* context
    ) noexcept {
        auto& buffer = *static_cast<ResponseBuffer*>(context);
        if (elementCount != 0 && elementSize >
            numeric_limits<size_t>::max() / elementCount) {
            buffer.overflowed = true;
            return 0;
        }
        const size_t byteCount = elementSize * elementCount;
        if (buffer.body.size() > buffer.maximumBytes ||
            byteCount > buffer.maximumBytes - buffer.body.size()) {
            buffer.overflowed = true;
            return 0;
        }
        try {
            buffer.body.append(data, byteCount);
        } catch (...) {
            buffer.overflowed = true;
            return 0;
        }
        buffer.metrics.downloadBytes = buffer.body.size();
        ++buffer.metrics.downloadChunks;
        return byteCount;
    }

    // Parses one response header line into the native header collection.
    static size_t writeHeader(
        char* data,
        size_t elementSize,
        size_t elementCount,
        void* context
    ) noexcept {
        auto& buffer = *static_cast<ResponseHeaderBuffer*>(context);
        if (elementCount != 0 && elementSize >
            numeric_limits<size_t>::max() / elementCount) {
            buffer.overflowed = true;
            return 0;
        }
        const size_t byteCount = elementSize * elementCount;
        if (buffer.receivedBytes > buffer.maximumBytes ||
            byteCount > buffer.maximumBytes - buffer.receivedBytes ||
            buffer.headers.size() >= buffer.maximumCount) {
            buffer.overflowed = true;
            return 0;
        }
        buffer.receivedBytes += byteCount;
        try {
            string line(data, byteCount);
            const size_t separator = line.find(':');
            if (separator == string::npos) {
                return byteCount;
            }
            string name = trim(line.substr(0, separator));
            string value = trim(line.substr(separator + 1));
            if (!name.empty()) {
                buffer.headers.emplace_back(std::move(name), std::move(value));
            }
        } catch (...) {
            return 0;
        }
        return byteCount;
    }

    // Removes HTTP whitespace from both sides of a header fragment.
    static string trim(string value) {
        const size_t start = value.find_first_not_of(" \t\r\n");
        if (start == string::npos) {
            return "";
        }
        const size_t end = value.find_last_not_of(" \t\r\n");
        return value.substr(start, end - start + 1);
    }

    static bool isSensitiveHeader(const string& name) noexcept {
        static constexpr const char* SensitiveHeaders[] = {
            "authorization",
            "proxy-authorization",
            "cookie",
            "set-cookie",
            "x-api-key",
            "api-key",
            "x-auth-token",
            "x-access-token",
            "x-refresh-token"
        };
        for (const char* sensitiveName : SensitiveHeaders) {
            if (name.size() != string(sensitiveName).size()) continue;
            bool equal = true;
            for (size_t index = 0; index < name.size(); ++index) {
                const char left = name[index] >= 'A' && name[index] <= 'Z'
                    ? static_cast<char>(name[index] - 'A' + 'a')
                    : name[index];
                if (left != sensitiveName[index]) {
                    equal = false;
                    break;
                }
            }
            if (equal) return true;
        }
        return false;
    }

    static bool containsSensitiveHeader(
        const vector<HttpHeader>& headers
    ) noexcept {
        for (const HttpHeader& header : headers) {
            if (isSensitiveHeader(header.getName())) return true;
        }
        return false;
    }

    // Acquires one reusable easy handle without exceeding pool bounds.
    CURL* acquireHandle(const runtime::RequestHandle& requestHandle) {
        unique_lock lock(mutex_);
        while (!stopping_ && availableHandles_.empty()) {
            requestHandle.throwIfCancellationRequested();
            available_.wait_for(lock, chrono::milliseconds(25));
        }
        if (stopping_) {
            throw runtime::CrossaException(
                runtime::CrossaError::runtime(
                    "Native HTTP transport is shutting down."
                )
            );
        }
        requestHandle.throwIfCancellationRequested();
        CURL* handle = availableHandles_.back();
        availableHandles_.pop_back();
        activeRequests_.emplace(handle, requestHandle);
        return handle;
    }

    // Returns one easy handle to the reusable bounded pool.
    void releaseHandle(CURL* handle) noexcept {
        {
            lock_guard lock(mutex_);
            activeRequests_.erase(handle);
            availableHandles_.push_back(handle);
            if (activeRequests_.empty()) {
                drained_.notify_all();
            }
        }
        available_.notify_one();
    }

    // Configures and performs one libcurl transfer.
    response::HttpResponse perform(
        CURL* handle,
        const request::PreparedRequest& request,
        const runtime::RequestHandle& requestHandle
    ) {
        curl_easy_reset(handle);
        ResponseBuffer responseBuffer{
            "",
            request.getMaximumResponseBytes(),
            false,
            {}
        };
        ResponseHeaderBuffer responseHeaderBuffer{
            {},
            request.getMaximumResponseHeaderBytes(),
            request.getMaximumResponseHeaderCount(),
            0,
            false
        };
        array<char, CURL_ERROR_SIZE> errorBuffer{};
        ProgressContext progressContext{
            &requestHandle,
            &responseBuffer.metrics
        };
        curl_slist* requestHeaders = createHeaders(request.getHeaders());
        curl_mime* multipart = nullptr;
        CURLcode result = CURLE_OK;
        try {
            multipart = createMultipart(handle, request);
            setCommonOptions(
                handle,
                request,
                responseBuffer,
                responseHeaderBuffer,
                errorBuffer,
                requestHeaders,
                progressContext
            );
            setMethodOptions(handle, request, multipart);
            result = curl_easy_perform(handle);
        } catch (...) {
            if (multipart != nullptr) {
                curl_mime_free(multipart);
            }
            curl_slist_free_all(requestHeaders);
            throw;
        }
        if (multipart != nullptr) {
            curl_mime_free(multipart);
        }
        curl_slist_free_all(requestHeaders);

        if (responseBuffer.overflowed) {
            throw runtime::CrossaException(
                runtime::CrossaError::runtime(
                    "HTTP response exceeded the configured byte limit."
                )
            );
        }
        if (responseHeaderBuffer.overflowed) {
            throw runtime::CrossaException(
                runtime::CrossaError::runtime(
                    "HTTP response headers exceeded the configured limit."
                )
            );
        }
        if (result != CURLE_OK) {
            const string detail = errorBuffer.front() == '\0'
                ? curl_easy_strerror(result)
                : errorBuffer.data();
            throwTransportError(result, detail, requestHandle);
        }

        long statusCode = 0;
        if (curl_easy_getinfo(
                handle,
                CURLINFO_RESPONSE_CODE,
                &statusCode
            ) != CURLE_OK) {
            throw runtime::CrossaException(
                runtime::CrossaError::connection(
                    "Unable to read the HTTP response status."
                )
            );
        }
        return response::HttpResponse(
            statusCode,
            std::move(responseHeaderBuffer.headers),
            std::move(responseBuffer.body),
            responseBuffer.metrics
        );
    }

    // Applies URL, bounds, callbacks, TLS, redirect, and reuse options.
    static void setCommonOptions(
        CURL* handle,
        const request::PreparedRequest& request,
        ResponseBuffer& responseBuffer,
        ResponseHeaderBuffer& responseHeaders,
        array<char, CURL_ERROR_SIZE>& errorBuffer,
        curl_slist* requestHeaders,
        ProgressContext& progressContext
    ) {
        setOption(handle, CURLOPT_URL, request.getUrl().c_str());
        setOption(handle, CURLOPT_HTTPHEADER, requestHeaders);
        setOption(handle, CURLOPT_ERRORBUFFER, errorBuffer.data());
        setOption(handle, CURLOPT_NOSIGNAL, 1L);
        setOption(handle, CURLOPT_TIMEOUT_MS, request.getTimeoutMilliseconds());
        setOption(handle, CURLOPT_CONNECTTIMEOUT_MS, request.getTimeoutMilliseconds());
        const bool allowRedirects = request.shouldFollowRedirects() &&
            !containsSensitiveHeader(request.getHeaders());
        setOption(handle, CURLOPT_FOLLOWLOCATION, allowRedirects ? 1L : 0L);
        setOption(handle, CURLOPT_MAXREDIRS, 5L);
#if LIBCURL_VERSION_NUM >= 0x075500
        setOption(handle, CURLOPT_PROTOCOLS_STR, "http,https");
        setOption(
            handle,
            CURLOPT_REDIR_PROTOCOLS_STR,
            request.getUrl().starts_with("https://") ? "https" : "http,https"
        );
#else
        setOption(
            handle,
            CURLOPT_PROTOCOLS,
            static_cast<long>(CURLPROTO_HTTP | CURLPROTO_HTTPS)
        );
        setOption(
            handle,
            CURLOPT_REDIR_PROTOCOLS,
            request.getUrl().starts_with("https://")
                ? static_cast<long>(CURLPROTO_HTTPS)
                : static_cast<long>(CURLPROTO_HTTP | CURLPROTO_HTTPS)
        );
#endif
        setOption(handle, CURLOPT_SSL_VERIFYPEER, 1L);
        setOption(handle, CURLOPT_SSL_VERIFYHOST, 2L);
        setOption(handle, CURLOPT_TCP_KEEPALIVE, 1L);
        setOption(handle, CURLOPT_ACCEPT_ENCODING, "");
        setOption(handle, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_2TLS);
        const NetworkPolicy::Proxy proxy = NetworkPolicy::parseProxy(
            request.getProxy()
        );
        if (!proxy.url.empty()) {
            setOption(handle, CURLOPT_PROXY, proxy.url.c_str());
            if (!proxy.username.empty()) {
                setOption(handle, CURLOPT_PROXYUSERNAME, proxy.username.c_str());
            }
            if (!proxy.password.empty()) {
                setOption(handle, CURLOPT_PROXYPASSWORD, proxy.password.c_str());
            }
        } else {
            setOption(handle, CURLOPT_PROXY, nullptr);
            setOption(handle, CURLOPT_PROXYUSERNAME, nullptr);
            setOption(handle, CURLOPT_PROXYPASSWORD, nullptr);
        }
        const NetworkPolicy::Certificate certificate =
            NetworkPolicy::parseCertificate(request.getCertificatePolicy());
        setOption(handle, CURLOPT_SSL_VERIFYPEER, 1L);
        setOption(handle, CURLOPT_SSL_VERIFYHOST, 2L);
#if defined(CROSSA_ANDROID_EMBEDDED_CA_BUNDLE)
        curl_blob caBundle{
            const_cast<char*>(kCrossaAndroidCaBundle),
            kCrossaAndroidCaBundleSize,
            CURL_BLOB_NOCOPY
        };
        setOption(handle, CURLOPT_CAINFO_BLOB, &caBundle);
#endif
        if (!certificate.caInfo.empty()) {
            setOption(handle, CURLOPT_CAINFO, certificate.caInfo.c_str());
        }
#if !defined(CROSSA_ANDROID_EMBEDDED_CA_BUNDLE)
        else {
            setOption(handle, CURLOPT_CAINFO, nullptr);
        }
#endif
        if (!certificate.clientCertificate.empty()) {
            setOption(handle, CURLOPT_SSLCERT, certificate.clientCertificate.c_str());
        } else {
            setOption(handle, CURLOPT_SSLCERT, nullptr);
        }
        if (!certificate.clientKey.empty()) {
            setOption(handle, CURLOPT_SSLKEY, certificate.clientKey.c_str());
        } else {
            setOption(handle, CURLOPT_SSLKEY, nullptr);
        }
#if LIBCURL_VERSION_NUM >= 0x072700
        if (!certificate.pinnedPublicKey.empty()) {
            setOption(handle, CURLOPT_PINNEDPUBLICKEY, certificate.pinnedPublicKey.c_str());
        } else {
            setOption(handle, CURLOPT_PINNEDPUBLICKEY, nullptr);
        }
#endif
        setOption(handle, CURLOPT_WRITEFUNCTION, writeBody);
        setOption(handle, CURLOPT_WRITEDATA, &responseBuffer);
        setOption(handle, CURLOPT_HEADERFUNCTION, writeHeader);
        setOption(handle, CURLOPT_HEADERDATA, &responseHeaders);
        setOption(handle, CURLOPT_NOPROGRESS, 0L);
        setOption(handle, CURLOPT_XFERINFOFUNCTION, observeProgress);
        responseBuffer.metrics.streamed = request.getDownloadStreaming()
            .value_or(false);
        setOption(handle, CURLOPT_XFERINFODATA, &progressContext);
        if (!request.getUploadProgress().value_or(false)) {
            setOption(handle, CURLOPT_NOPROGRESS, 1L);
        }
    }

    template<typename Value>
    static void setOption(CURL* handle, CURLoption option, Value value) {
        const CURLcode result = curl_easy_setopt(handle, option, value);
        if (result != CURLE_OK) {
            throw runtime::CrossaException(
                runtime::CrossaError::runtime(
                    "Unable to configure native HTTP transport option."
                )
            );
        }
    }

    // Maps one libcurl failure into the stable CrossaError taxonomy.
    [[noreturn]] static void throwTransportError(
        CURLcode result,
        const string& detail,
        const runtime::RequestHandle& requestHandle
    ) {
        const long nativeCode = static_cast<long>(result);
        if (result == CURLE_ABORTED_BY_CALLBACK &&
            requestHandle.isCancellationRequested()) {
            throw runtime::CrossaException(
                runtime::CrossaError::cancellation()
            );
        }
        if (result == CURLE_OPERATION_TIMEDOUT) {
            throw runtime::CrossaException(
                runtime::CrossaError::timeout(
                    "Native HTTP request timed out: " + detail
                )
            );
        }
        switch (result) {
            case CURLE_SSL_CONNECT_ERROR:
            case CURLE_PEER_FAILED_VERIFICATION:
            case CURLE_SSL_CERTPROBLEM:
            case CURLE_SSL_CIPHER:
            case CURLE_SSL_CACERT_BADFILE:
            case CURLE_SSL_CRL_BADFILE:
            case CURLE_SSL_ISSUER_ERROR:
                throw runtime::CrossaException(
                    runtime::CrossaError::tls(
                        "Native TLS request failed: " + detail,
                        nativeCode
                    )
                );
            case CURLE_COULDNT_RESOLVE_PROXY:
            case CURLE_COULDNT_RESOLVE_HOST:
            case CURLE_COULDNT_CONNECT:
            case CURLE_SEND_ERROR:
            case CURLE_RECV_ERROR:
            case CURLE_GOT_NOTHING:
                throw runtime::CrossaException(
                    runtime::CrossaError::connection(
                        "Native HTTP connection failed: " + detail,
                        nativeCode
                    )
                );
            default:
                throw runtime::CrossaException(
                    runtime::CrossaError::connection(
                        "Native HTTP transport failed: " + detail,
                        nativeCode
                    )
                );
        }
    }

    // Applies the exact HTTP method and optional serialized request body.
    static void setMethodOptions(
        CURL* handle,
        const request::PreparedRequest& request,
        curl_mime* multipart
    ) {
        const HttpMethod method = request.getMethod();
        if (multipart != nullptr && method != HttpMethod::Head) {
            setOption(handle, CURLOPT_MIMEPOST, multipart);
        } else if (request.getBody().has_value() && method != HttpMethod::Head) {
            setOption(
                handle,
                CURLOPT_POSTFIELDS,
                request.getBody()->data()
            );
            setOption(
                handle,
                CURLOPT_POSTFIELDSIZE_LARGE,
                static_cast<curl_off_t>(request.getBody()->size())
            );
        }
        setOption(
            handle,
            CURLOPT_CUSTOMREQUEST,
            HttpMethodUtils::toString(method).data()
        );
        if (method == HttpMethod::Head) {
            setOption(handle, CURLOPT_NOBODY, 1L);
        }
    }

    static curl_mime* createMultipart(
        CURL* handle,
        const request::PreparedRequest& request
    ) {
        if (!request.getMultipart().has_value()) {
            return nullptr;
        }
        const vector<NetworkPolicy::MultipartPart> parts =
            NetworkPolicy::parseMultipart(*request.getMultipart());
        curl_mime* mime = curl_mime_init(handle);
        if (mime == nullptr) {
            throw runtime_error("Unable to allocate multipart form.");
        }
        try {
            for (const NetworkPolicy::MultipartPart& part : parts) {
                curl_mimepart* item = curl_mime_addpart(mime);
                if (item == nullptr ||
                    curl_mime_name(item, part.name.c_str()) != CURLE_OK) {
                    throw runtime_error("Unable to allocate multipart field.");
                }
                CURLcode result = CURLE_OK;
                if (part.data.has_value()) {
                    result = curl_mime_data(
                        item,
                        part.data->data(),
                        part.data->size()
                    );
                } else {
                    result = curl_mime_filedata(
                        item,
                        part.filePath->c_str()
                    );
                }
                if (result != CURLE_OK) {
                    throw runtime_error("Unable to add multipart content.");
                }
                if (part.filename.has_value()) {
                    result = curl_mime_filename(item, part.filename->c_str());
                }
                if (result == CURLE_OK && part.contentType.has_value()) {
                    result = curl_mime_type(item, part.contentType->c_str());
                }
                if (result != CURLE_OK) {
                    throw runtime_error("Unable to configure multipart content.");
                }
            }
        } catch (...) {
            curl_mime_free(mime);
            throw;
        }
        return mime;
    }

    // Builds libcurl's native request-header linked list.
    static curl_slist* createHeaders(const vector<HttpHeader>& headers) {
        curl_slist* result = nullptr;
        for (const HttpHeader& header : headers) {
            const string value = header.getName() + ": " + header.getValue();
            curl_slist* appended = curl_slist_append(result, value.c_str());
            if (appended == nullptr) {
                curl_slist_free_all(result);
                throw runtime_error("Unable to allocate native HTTP headers.");
            }
            result = appended;
        }
        return result;
    }

    // Releases all easy handles owned by this transport.
    void releaseHandles() noexcept {
        for (CURL* handle : handles_) {
            curl_easy_cleanup(handle);
        }
        handles_.clear();
        availableHandles_.clear();
    }

    mutex mutex_;
    condition_variable available_;
    condition_variable drained_;
    vector<CURL*> handles_;
    vector<CURL*> availableHandles_;
    unordered_map<CURL*, runtime::RequestHandle> activeRequests_;
    bool stopping_;
};

// Initializes libcurl and creates the bounded reusable handle pool.
CurlTransport::CurlTransport(size_t poolSize)
    : implementation_(make_unique<Implementation>(poolSize)) {}

// Releases every reusable handle and the process libcurl state.
CurlTransport::~CurlTransport() = default;

// Executes one prepared request and returns its buffered native response.
response::HttpResponse CurlTransport::execute(
    const request::PreparedRequest& request,
    const runtime::RequestHandle& requestHandle
) {
    return implementation_->execute(request, requestHandle);
}

}
