#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "crossa/network/HttpHeader.h"

namespace crossa::network {

// Owns validated common networking limits and interceptor configuration.
// Its accessors configure request building, transport, parsing, and logging.
class NetworkConfiguration final {
public:
    // Creates the secure bounded default networking configuration.
    NetworkConfiguration();

    // Sets the base URL used only by relative request URLs.
    void setBaseUrl(std::string baseUrl);

    // Sets the default request timeout in milliseconds.
    void setTimeoutMilliseconds(std::int64_t timeoutMilliseconds);

    // Replaces common headers appended by the interceptor.
    void setCommonHeaders(std::vector<HttpHeader> headers);

    // Enables or disables the global interceptor.
    void setInterceptorEnabled(bool enabled) noexcept;

    // Enables privacy-aware request lifecycle logs.
    void setLogRequests(bool enabled) noexcept;

    // Enables privacy-aware response lifecycle logs.
    void setLogResponses(bool enabled) noexcept;

    // Enables or disables request and response header logging.
    void setLogHeaders(bool enabled) noexcept;

    // Enables or disables request and response body logging.
    void setLogBody(bool enabled) noexcept;

    // Replaces header names that must never appear in logs.
    void setExcludedLogHeaders(std::vector<std::string> headers);

    // Enables or disables bounded HTTP redirect following.
    void setFollowRedirects(bool enabled) noexcept;

    // Sets the maximum buffered response bytes.
    void setMaximumResponseBytes(std::size_t maximumResponseBytes);

    // Sets the maximum parsed JSON nesting depth.
    void setMaximumJsonDepth(std::size_t maximumJsonDepth);

    // Returns the configured base URL.
    [[nodiscard]] const std::string& getBaseUrl() const noexcept;

    // Returns the default request timeout in milliseconds.
    [[nodiscard]] std::int64_t getTimeoutMilliseconds() const noexcept;

    // Returns common interceptor headers.
    [[nodiscard]] const std::vector<HttpHeader>& getCommonHeaders() const noexcept;

    // Returns whether the global interceptor is enabled.
    [[nodiscard]] bool isInterceptorEnabled() const noexcept;

    // Returns whether request lifecycle logging is enabled.
    [[nodiscard]] bool shouldLogRequests() const noexcept;

    // Returns whether response lifecycle logging is enabled.
    [[nodiscard]] bool shouldLogResponses() const noexcept;

    // Returns whether request and response headers may be logged.
    [[nodiscard]] bool shouldLogHeaders() const noexcept;

    // Returns whether request and response bodies may be logged.
    [[nodiscard]] bool shouldLogBody() const noexcept;

    // Returns case-insensitive header names excluded from logs.
    [[nodiscard]] const std::vector<std::string>&
    getExcludedLogHeaders() const noexcept;

    // Returns whether redirects may be followed.
    [[nodiscard]] bool shouldFollowRedirects() const noexcept;

    // Returns the maximum buffered response bytes.
    [[nodiscard]] std::size_t getMaximumResponseBytes() const noexcept;

    // Returns the maximum parsed JSON nesting depth.
    [[nodiscard]] std::size_t getMaximumJsonDepth() const noexcept;

private:
    std::string baseUrl_;
    std::int64_t timeoutMilliseconds_;
    std::vector<HttpHeader> commonHeaders_;
    bool interceptorEnabled_;
    bool logRequests_;
    bool logResponses_;
    bool logHeaders_;
    bool logBody_;
    std::vector<std::string> excludedLogHeaders_;
    bool followRedirects_;
    std::size_t maximumResponseBytes_;
    std::size_t maximumJsonDepth_;
};

}
