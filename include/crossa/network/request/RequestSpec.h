#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "crossa/network/HttpHeader.h"
#include "crossa/network/HttpMethod.h"

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
        std::optional<std::int64_t> timeoutMilliseconds
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

private:
    HttpMethod method_;
    std::string url_;
    std::vector<HttpHeader> headers_;
    std::vector<HttpHeader> customHeaders_;
    QueryParameters queryParameters_;
    std::optional<std::string> body_;
    std::optional<std::int64_t> timeoutMilliseconds_;
};

}
