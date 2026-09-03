#include "crossa/network/request/PreparedRequest.h"

#include <utility>

using namespace std;

namespace crossa::network::request {

    // Creates one complete request with transport policy and limits.
    PreparedRequest::PreparedRequest(
        HttpMethod method,
        string url,
        vector<HttpHeader> headers,
        optional<string> body,
        int64_t timeoutMilliseconds,
        bool followRedirects,
        size_t maximumResponseBytes,
        size_t maximumResponseHeaderBytes,
        size_t maximumResponseHeaderCount,
        optional<json::JsonValue> retryPolicy,
        optional<json::JsonValue> multipart,
        optional<json::JsonValue> proxy,
        optional<json::JsonValue> certificatePolicy,
        optional<bool> uploadProgress,
        optional<bool> downloadStreaming,
        optional<json::JsonValue> telemetry
    )
        : method_(method),
          url_(std::move(url)),
          headers_(std::move(headers)),
          body_(std::move(body)),
          timeoutMilliseconds_(timeoutMilliseconds),
          followRedirects_(followRedirects),
          maximumResponseBytes_(maximumResponseBytes),
          maximumResponseHeaderBytes_(maximumResponseHeaderBytes),
          maximumResponseHeaderCount_(maximumResponseHeaderCount),
          retryPolicy_(std::move(retryPolicy)),
          multipart_(std::move(multipart)),
          proxy_(std::move(proxy)),
          certificatePolicy_(std::move(certificatePolicy)),
          uploadProgress_(uploadProgress),
          downloadStreaming_(downloadStreaming),
          telemetry_(std::move(telemetry)) {}

    PreparedRequest::PreparedRequest(
        HttpMethod method,
        string url,
        vector<HttpHeader> headers,
        optional<string> body,
        int64_t timeoutMilliseconds,
        bool followRedirects,
        size_t maximumResponseBytes,
        optional<json::JsonValue> retryPolicy,
        optional<json::JsonValue> multipart,
        optional<json::JsonValue> proxy,
        optional<json::JsonValue> certificatePolicy,
        optional<bool> uploadProgress,
        optional<bool> downloadStreaming,
        optional<json::JsonValue> telemetry
    )
        : PreparedRequest(
              std::move(method),
              std::move(url),
              std::move(headers),
              std::move(body),
              timeoutMilliseconds,
              followRedirects,
              maximumResponseBytes,
              1024U * 1024U,
              256U,
              std::move(retryPolicy),
              std::move(multipart),
              std::move(proxy),
              std::move(certificatePolicy),
              uploadProgress,
              downloadStreaming,
              std::move(telemetry)
          ) {}

    // Adds or replaces one header using case-insensitive name matching.
    void PreparedRequest::setHeader(HttpHeader header, bool overwrite) {
        for (HttpHeader& existing : headers_) {
            if (!headerNamesEqual(existing.getName(), header.getName())) {
                continue;
            }
            if (overwrite) {
                existing = std::move(header);
            }
            return;
        }
        headers_.push_back(std::move(header));
    }

    // Returns whether one header name is already present.
    bool PreparedRequest::hasHeader(const string& name) const noexcept {
        for (const HttpHeader& header : headers_) {
            if (headerNamesEqual(header.getName(), name)) {
                return true;
            }
        }
        return false;
    }

    // Returns the HTTP method.
    HttpMethod PreparedRequest::getMethod() const noexcept {
        return method_;
    }

    // Returns the final absolute URL.
    const string& PreparedRequest::getUrl() const noexcept {
        return url_;
    }

    // Returns the final ordered headers.
    const vector<HttpHeader>& PreparedRequest::getHeaders() const noexcept {
        return headers_;
    }

    // Returns the serialized body or null when absent.
    const optional<string>& PreparedRequest::getBody() const noexcept {
        return body_;
    }

    // Returns the effective timeout in milliseconds.
    int64_t PreparedRequest::getTimeoutMilliseconds() const noexcept {
        return timeoutMilliseconds_;
    }

    // Returns whether redirects may be followed.
    bool PreparedRequest::shouldFollowRedirects() const noexcept {
        return followRedirects_;
    }

    // Returns the maximum response body bytes.
    size_t PreparedRequest::getMaximumResponseBytes() const noexcept {
        return maximumResponseBytes_;
    }

    size_t PreparedRequest::getMaximumResponseHeaderBytes() const noexcept {
        return maximumResponseHeaderBytes_;
    }

    size_t PreparedRequest::getMaximumResponseHeaderCount() const noexcept {
        return maximumResponseHeaderCount_;
    }

    const optional<json::JsonValue>&
    PreparedRequest::getRetryPolicy() const noexcept {
        return retryPolicy_;
    }

    const optional<json::JsonValue>&
    PreparedRequest::getMultipart() const noexcept {
        return multipart_;
    }

    const optional<json::JsonValue>& PreparedRequest::getProxy() const noexcept {
        return proxy_;
    }

    const optional<json::JsonValue>&
    PreparedRequest::getCertificatePolicy() const noexcept {
        return certificatePolicy_;
    }

    const optional<bool>& PreparedRequest::getUploadProgress() const noexcept {
        return uploadProgress_;
    }

    const optional<bool>&
    PreparedRequest::getDownloadStreaming() const noexcept {
        return downloadStreaming_;
    }

    const optional<json::JsonValue>& PreparedRequest::getTelemetry() const noexcept {
        return telemetry_;
    }

    // Compares header names without ASCII case sensitivity.
    bool PreparedRequest::headerNamesEqual(
        const string& left,
        const string& right
    ) noexcept {
        if (left.size() != right.size()) {
            return false;
        }
        for (size_t index = 0; index < left.size(); ++index) {
            const char leftValue = left[index] >= 'A' && left[index] <= 'Z'
                ? static_cast<char>(left[index] - 'A' + 'a')
                : left[index];
            const char rightValue = right[index] >= 'A' && right[index] <= 'Z'
                ? static_cast<char>(right[index] - 'A' + 'a')
                : right[index];
            if (leftValue != rightValue) {
                return false;
            }
        }
        return true;
    }

}
