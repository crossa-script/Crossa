#include "crossa/network/transport/CurlTransport.h"

#include <array>
#include <condition_variable>
#include <curl/curl.h>
#include <mutex>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "crossa/network/HttpHeader.h"
#include "crossa/network/HttpMethod.h"

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
            lock_guard lock(mutex_);
            stopping_ = true;
        }
        available_.notify_all();
        releaseHandles();
        curl_global_cleanup();
    }

    // Performs one request using an acquired reusable easy handle.
    response::HttpResponse execute(const request::PreparedRequest& request) {
        CURL* handle = acquireHandle();
        try {
            response::HttpResponse response = perform(handle, request);
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
    };

    // Appends response bytes while enforcing the configured size limit.
    static size_t writeBody(
        char* data,
        size_t elementSize,
        size_t elementCount,
        void* context
    ) noexcept {
        auto& buffer = *static_cast<ResponseBuffer*>(context);
        const size_t byteCount = elementSize * elementCount;
        if (byteCount > buffer.maximumBytes - buffer.body.size()) {
            buffer.overflowed = true;
            return 0;
        }
        buffer.body.append(data, byteCount);
        return byteCount;
    }

    // Parses one response header line into the native header collection.
    static size_t writeHeader(
        char* data,
        size_t elementSize,
        size_t elementCount,
        void* context
    ) noexcept {
        const size_t byteCount = elementSize * elementCount;
        auto& headers = *static_cast<vector<HttpHeader>*>(context);
        try {
            string line(data, byteCount);
            const size_t separator = line.find(':');
            if (separator == string::npos) {
                return byteCount;
            }
            string name = trim(line.substr(0, separator));
            string value = trim(line.substr(separator + 1));
            if (!name.empty()) {
                headers.emplace_back(std::move(name), std::move(value));
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

    // Acquires one reusable easy handle without exceeding pool bounds.
    CURL* acquireHandle() {
        unique_lock lock(mutex_);
        available_.wait(lock, [this]() {
            return stopping_ || !availableHandles_.empty();
        });
        if (stopping_) {
            throw runtime_error("Native HTTP transport is shutting down.");
        }
        CURL* handle = availableHandles_.back();
        availableHandles_.pop_back();
        return handle;
    }

    // Returns one easy handle to the reusable bounded pool.
    void releaseHandle(CURL* handle) noexcept {
        {
            lock_guard lock(mutex_);
            availableHandles_.push_back(handle);
        }
        available_.notify_one();
    }

    // Configures and performs one libcurl transfer.
    response::HttpResponse perform(
        CURL* handle,
        const request::PreparedRequest& request
    ) {
        curl_easy_reset(handle);
        ResponseBuffer responseBuffer{
            "",
            request.getMaximumResponseBytes(),
            false
        };
        vector<HttpHeader> responseHeaders;
        array<char, CURL_ERROR_SIZE> errorBuffer{};
        curl_slist* requestHeaders = createHeaders(request.getHeaders());

        setCommonOptions(
            handle,
            request,
            responseBuffer,
            responseHeaders,
            errorBuffer,
            requestHeaders
        );
        setMethodOptions(handle, request);
        const CURLcode result = curl_easy_perform(handle);
        curl_slist_free_all(requestHeaders);

        if (responseBuffer.overflowed) {
            throw runtime_error("HTTP response exceeded the configured byte limit.");
        }
        if (result != CURLE_OK) {
            const string detail = errorBuffer.front() == '\0'
                ? curl_easy_strerror(result)
                : errorBuffer.data();
            throw runtime_error("Native HTTP transport failed: " + detail);
        }

        long statusCode = 0;
        if (curl_easy_getinfo(
                handle,
                CURLINFO_RESPONSE_CODE,
                &statusCode
            ) != CURLE_OK) {
            throw runtime_error("Unable to read the HTTP response status.");
        }
        return response::HttpResponse(
            statusCode,
            std::move(responseHeaders),
            std::move(responseBuffer.body)
        );
    }

    // Applies URL, bounds, callbacks, TLS, redirect, and reuse options.
    static void setCommonOptions(
        CURL* handle,
        const request::PreparedRequest& request,
        ResponseBuffer& responseBuffer,
        vector<HttpHeader>& responseHeaders,
        array<char, CURL_ERROR_SIZE>& errorBuffer,
        curl_slist* requestHeaders
    ) {
        curl_easy_setopt(handle, CURLOPT_URL, request.getUrl().c_str());
        curl_easy_setopt(handle, CURLOPT_HTTPHEADER, requestHeaders);
        curl_easy_setopt(handle, CURLOPT_ERRORBUFFER, errorBuffer.data());
        curl_easy_setopt(handle, CURLOPT_NOSIGNAL, 1L);
        curl_easy_setopt(handle, CURLOPT_TIMEOUT_MS,
                         request.getTimeoutMilliseconds());
        curl_easy_setopt(handle, CURLOPT_CONNECTTIMEOUT_MS,
                         request.getTimeoutMilliseconds());
        curl_easy_setopt(handle, CURLOPT_FOLLOWLOCATION,
                         request.shouldFollowRedirects() ? 1L : 0L);
        curl_easy_setopt(handle, CURLOPT_MAXREDIRS, 5L);
#if LIBCURL_VERSION_NUM >= 0x075500
        curl_easy_setopt(handle, CURLOPT_PROTOCOLS_STR, "http,https");
        curl_easy_setopt(handle, CURLOPT_REDIR_PROTOCOLS_STR, "http,https");
#else
        curl_easy_setopt(
            handle,
            CURLOPT_PROTOCOLS,
            static_cast<long>(CURLPROTO_HTTP | CURLPROTO_HTTPS)
        );
        curl_easy_setopt(
            handle,
            CURLOPT_REDIR_PROTOCOLS,
            static_cast<long>(CURLPROTO_HTTP | CURLPROTO_HTTPS)
        );
#endif
        curl_easy_setopt(handle, CURLOPT_SSL_VERIFYPEER, 1L);
        curl_easy_setopt(handle, CURLOPT_SSL_VERIFYHOST, 2L);
        curl_easy_setopt(handle, CURLOPT_TCP_KEEPALIVE, 1L);
        curl_easy_setopt(handle, CURLOPT_ACCEPT_ENCODING, "");
        curl_easy_setopt(handle, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_2TLS);
        curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, writeBody);
        curl_easy_setopt(handle, CURLOPT_WRITEDATA, &responseBuffer);
        curl_easy_setopt(handle, CURLOPT_HEADERFUNCTION, writeHeader);
        curl_easy_setopt(handle, CURLOPT_HEADERDATA, &responseHeaders);
    }

    // Applies the exact HTTP method and optional serialized request body.
    static void setMethodOptions(
        CURL* handle,
        const request::PreparedRequest& request
    ) {
        const HttpMethod method = request.getMethod();
        if (request.getBody().has_value() && method != HttpMethod::Head) {
            curl_easy_setopt(
                handle,
                CURLOPT_POSTFIELDS,
                request.getBody()->data()
            );
            curl_easy_setopt(
                handle,
                CURLOPT_POSTFIELDSIZE_LARGE,
                static_cast<curl_off_t>(request.getBody()->size())
            );
        }
        curl_easy_setopt(
            handle,
            CURLOPT_CUSTOMREQUEST,
            HttpMethodUtils::toString(method).data()
        );
        if (method == HttpMethod::Head) {
            curl_easy_setopt(handle, CURLOPT_NOBODY, 1L);
        }
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
    vector<CURL*> handles_;
    vector<CURL*> availableHandles_;
    bool stopping_;
};

// Initializes libcurl and creates the bounded reusable handle pool.
CurlTransport::CurlTransport(size_t poolSize)
    : implementation_(make_unique<Implementation>(poolSize)) {}

// Releases every reusable handle and the process libcurl state.
CurlTransport::~CurlTransport() = default;

// Executes one prepared request and returns its buffered native response.
response::HttpResponse CurlTransport::execute(
    const request::PreparedRequest& request
) {
    return implementation_->execute(request);
}

}
