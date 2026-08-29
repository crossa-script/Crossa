#include "crossa/network/request/RequestSpec.h"

#include <utility>

using namespace std;

namespace crossa::network::request {

    // Creates one evaluated request specification.
    RequestSpec::RequestSpec(
        HttpMethod method,
        string url,
        vector<HttpHeader> headers,
        vector<HttpHeader> customHeaders,
        QueryParameters queryParameters,
        optional<string> body,
        optional<int64_t> timeoutMilliseconds,
        optional<json::JsonValue> retryPolicy,
        optional<json::JsonValue> authentication,
        optional<json::JsonValue> multipart,
        optional<bool> uploadProgress,
        optional<bool> downloadStreaming,
        optional<bool> coalesce,
        optional<json::JsonValue> proxy,
        optional<json::JsonValue> certificatePolicy,
        optional<json::JsonValue> telemetry
    )
        : method_(method),
          url_(std::move(url)),
          headers_(std::move(headers)),
          customHeaders_(std::move(customHeaders)),
          queryParameters_(std::move(queryParameters)),
          body_(std::move(body)),
          timeoutMilliseconds_(timeoutMilliseconds),
          retryPolicy_(std::move(retryPolicy)),
          authentication_(std::move(authentication)),
          multipart_(std::move(multipart)),
          uploadProgress_(uploadProgress),
          downloadStreaming_(downloadStreaming),
          coalesce_(coalesce),
          proxy_(std::move(proxy)),
          certificatePolicy_(std::move(certificatePolicy)),
          telemetry_(std::move(telemetry)) {}

    // Returns the HTTP method.
    HttpMethod RequestSpec::getMethod() const noexcept {
        return method_;
    }

    // Returns the absolute or relative URL input.
    const string& RequestSpec::getUrl() const noexcept {
        return url_;
    }

    // Returns ordinary request headers.
    const vector<HttpHeader>& RequestSpec::getHeaders() const noexcept {
        return headers_;
    }

    // Returns overriding custom request headers.
    const vector<HttpHeader>& RequestSpec::getCustomHeaders() const noexcept {
        return customHeaders_;
    }

    // Returns ordered query parameters.
    const RequestSpec::QueryParameters&
    RequestSpec::getQueryParameters() const noexcept {
        return queryParameters_;
    }

    // Returns the serialized JSON body or null when absent.
    const optional<string>& RequestSpec::getBody() const noexcept {
        return body_;
    }

    // Returns the request-specific timeout or null when absent.
    const optional<int64_t>&
    RequestSpec::getTimeoutMilliseconds() const noexcept {
        return timeoutMilliseconds_;
    }

    const optional<json::JsonValue>& RequestSpec::getRetryPolicy() const noexcept {
        return retryPolicy_;
    }

    const optional<json::JsonValue>& RequestSpec::getAuthentication() const noexcept {
        return authentication_;
    }

    const optional<json::JsonValue>& RequestSpec::getMultipart() const noexcept {
        return multipart_;
    }

    const optional<bool>& RequestSpec::getUploadProgress() const noexcept {
        return uploadProgress_;
    }

    const optional<bool>& RequestSpec::getDownloadStreaming() const noexcept {
        return downloadStreaming_;
    }

    const optional<bool>& RequestSpec::getCoalesce() const noexcept {
        return coalesce_;
    }

    const optional<json::JsonValue>& RequestSpec::getProxy() const noexcept {
        return proxy_;
    }

    const optional<json::JsonValue>&
    RequestSpec::getCertificatePolicy() const noexcept {
        return certificatePolicy_;
    }

    const optional<json::JsonValue>& RequestSpec::getTelemetry() const noexcept {
        return telemetry_;
    }

}
