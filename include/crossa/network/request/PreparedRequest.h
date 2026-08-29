#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "crossa/network/HttpHeader.h"
#include "crossa/network/HttpMethod.h"

namespace crossa::network::request {

// Owns the complete validated HTTP request consumed by transport.
// setHeader() enables deterministic interceptor/header precedence.
class PreparedRequest final {
public:
    // Creates one complete request with transport policy and limits.
    PreparedRequest(
        HttpMethod method,
        std::string url,
        std::vector<HttpHeader> headers,
        std::optional<std::string> body,
        std::int64_t timeoutMilliseconds,
        bool followRedirects,
        std::size_t maximumResponseBytes
    );

    // Adds or replaces one header using case-insensitive name matching.
    void setHeader(HttpHeader header, bool overwrite);

    // Returns whether one header name is already present.
    [[nodiscard]] bool hasHeader(const std::string& name) const noexcept;

    // Returns the HTTP method.
    [[nodiscard]] HttpMethod getMethod() const noexcept;

    // Returns the final absolute URL.
    [[nodiscard]] const std::string& getUrl() const noexcept;

    // Returns the final ordered headers.
    [[nodiscard]] const std::vector<HttpHeader>& getHeaders() const noexcept;

    // Returns the serialized body or null when absent.
    [[nodiscard]] const std::optional<std::string>& getBody() const noexcept;

    // Returns the effective timeout in milliseconds.
    [[nodiscard]] std::int64_t getTimeoutMilliseconds() const noexcept;

    // Returns whether redirects may be followed.
    [[nodiscard]] bool shouldFollowRedirects() const noexcept;

    // Returns the maximum response body bytes.
    [[nodiscard]] std::size_t getMaximumResponseBytes() const noexcept;

private:
    // Compares header names without ASCII case sensitivity.
    [[nodiscard]] static bool headerNamesEqual(
        const std::string& left,
        const std::string& right
    ) noexcept;

    HttpMethod method_;
    std::string url_;
    std::vector<HttpHeader> headers_;
    std::optional<std::string> body_;
    std::int64_t timeoutMilliseconds_;
    bool followRedirects_;
    std::size_t maximumResponseBytes_;
};

}
