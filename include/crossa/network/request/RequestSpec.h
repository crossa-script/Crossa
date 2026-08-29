#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "crossa/network/HttpHeader.h"
#include "crossa/network/HttpMethod.h"
#include "crossa/network/json/JsonValue.h"

namespace crossa::network::request {

// Owns evaluated request inputs before URL/header policy is applied.
// Its fields separate normal headers, overriding headers, query, body, and timeout.
class RequestSpec final {
public:
    using QueryParameters = std::vector<std::pair<std::string, std::string>>;

    // Creates one evaluated request specification.
    RequestSpec(
        HttpMethod method,
        std::string url,
        std::vector<HttpHeader> headers,
        std::vector<HttpHeader> customHeaders,
        QueryParameters queryParameters,
        std::optional<std::string> body,
        std::optional<std::int64_t> timeoutMilliseconds,
        std::optional<json::JsonValue> retryPolicy,
        std::optional<json::JsonValue> authentication,
        std::optional<json::JsonValue> multipart,
        std::optional<bool> uploadProgress,
        std::optional<bool> downloadStreaming,
        std::optional<bool> coalesce,
        std::optional<json::JsonValue> proxy,
        std::optional<json::JsonValue> certificatePolicy,
        std::optional<json::JsonValue> telemetry
    );

    // Returns the HTTP method.
    [[nodiscard]] HttpMethod getMethod() const noexcept;

    // Returns the absolute or relative URL input.
    [[nodiscard]] const std::string& getUrl() const noexcept;

    // Returns ordinary request headers.
    [[nodiscard]] const std::vector<HttpHeader>& getHeaders() const noexcept;

    // Returns overriding custom request headers.
    [[nodiscard]] const std::vector<HttpHeader>&
    getCustomHeaders() const noexcept;

    // Returns ordered query parameters.
    [[nodiscard]] const QueryParameters& getQueryParameters() const noexcept;

    // Returns the serialized JSON body or null when absent.
    [[nodiscard]] const std::optional<std::string>& getBody() const noexcept;

    // Returns the request-specific timeout or null when absent.
    [[nodiscard]] const std::optional<std::int64_t>&
    getTimeoutMilliseconds() const noexcept;

    [[nodiscard]] const std::optional<json::JsonValue>&
    getRetryPolicy() const noexcept;

    [[nodiscard]] const std::optional<json::JsonValue>&
    getAuthentication() const noexcept;

    [[nodiscard]] const std::optional<json::JsonValue>&
    getMultipart() const noexcept;

    [[nodiscard]] const std::optional<bool>&
    getUploadProgress() const noexcept;

    [[nodiscard]] const std::optional<bool>&
    getDownloadStreaming() const noexcept;

    [[nodiscard]] const std::optional<bool>& getCoalesce() const noexcept;

    [[nodiscard]] const std::optional<json::JsonValue>&
    getProxy() const noexcept;

    [[nodiscard]] const std::optional<json::JsonValue>&
    getCertificatePolicy() const noexcept;

    [[nodiscard]] const std::optional<json::JsonValue>&
    getTelemetry() const noexcept;

private:
    HttpMethod method_;
    std::string url_;
    std::vector<HttpHeader> headers_;
    std::vector<HttpHeader> customHeaders_;
    QueryParameters queryParameters_;
    std::optional<std::string> body_;
    std::optional<std::int64_t> timeoutMilliseconds_;
    std::optional<json::JsonValue> retryPolicy_;
    std::optional<json::JsonValue> authentication_;
    std::optional<json::JsonValue> multipart_;
    std::optional<bool> uploadProgress_;
    std::optional<bool> downloadStreaming_;
    std::optional<bool> coalesce_;
    std::optional<json::JsonValue> proxy_;
    std::optional<json::JsonValue> certificatePolicy_;
    std::optional<json::JsonValue> telemetry_;
};

}
