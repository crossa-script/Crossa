#include "crossa/network/NetworkConfiguration.h"

#include <stdexcept>
#include <utility>

#include "crossa/network/utils/UrlUtils.h"

using namespace std;

namespace crossa::network {

    // Creates the secure bounded default networking configuration.
    NetworkConfiguration::NetworkConfiguration()
        : timeoutMilliseconds_(30000),
          interceptorEnabled_(true),
          logRequests_(true),
          logResponses_(true),
          followRedirects_(true),
          maximumResponseBytes_(8U * 1024U * 1024U),
          maximumJsonDepth_(128) {}

    // Sets the base URL used only by relative request URLs.
    void NetworkConfiguration::setBaseUrl(string baseUrl) {
        if (!baseUrl.empty() && !utils::UrlUtils::isAbsoluteHttpUrl(baseUrl)) {
            throw invalid_argument("Network baseUrl must use HTTP or HTTPS.");
        }
        baseUrl_ = std::move(baseUrl);
    }

    // Sets the default request timeout in milliseconds.
    void NetworkConfiguration::setTimeoutMilliseconds(
        int64_t timeoutMilliseconds
    ) {
        if (timeoutMilliseconds <= 0 || timeoutMilliseconds > 600000) {
            throw invalid_argument(
                "Network timeout must be between 1 and 600000 milliseconds."
            );
        }
        timeoutMilliseconds_ = timeoutMilliseconds;
    }

    // Replaces common headers appended by the interceptor.
    void NetworkConfiguration::setCommonHeaders(vector<HttpHeader> headers) {
        for (const HttpHeader& header : headers) {
            if (header.getName().empty() ||
                header.getName().find(':') != string::npos ||
                header.getName().find('\r') != string::npos ||
                header.getName().find('\n') != string::npos ||
                header.getValue().find('\r') != string::npos ||
                header.getValue().find('\n') != string::npos) {
                throw invalid_argument("Invalid common HTTP header.");
            }
        }
        commonHeaders_ = std::move(headers);
    }

    // Enables or disables the global interceptor.
    void NetworkConfiguration::setInterceptorEnabled(bool enabled) noexcept {
        interceptorEnabled_ = enabled;
    }

    // Enables privacy-aware request lifecycle logs.
    void NetworkConfiguration::setLogRequests(bool enabled) noexcept {
        logRequests_ = enabled;
    }

    // Enables privacy-aware response lifecycle logs.
    void NetworkConfiguration::setLogResponses(bool enabled) noexcept {
        logResponses_ = enabled;
    }

    // Enables or disables bounded HTTP redirect following.
    void NetworkConfiguration::setFollowRedirects(bool enabled) noexcept {
        followRedirects_ = enabled;
    }

    // Sets the maximum buffered response bytes.
    void NetworkConfiguration::setMaximumResponseBytes(
        size_t maximumResponseBytes
    ) {
        if (maximumResponseBytes == 0 ||
            maximumResponseBytes > 256U * 1024U * 1024U) {
            throw invalid_argument(
                "Maximum response bytes must be between 1 and 268435456."
            );
        }
        maximumResponseBytes_ = maximumResponseBytes;
    }

    // Sets the maximum parsed JSON nesting depth.
    void NetworkConfiguration::setMaximumJsonDepth(size_t maximumJsonDepth) {
        if (maximumJsonDepth == 0 || maximumJsonDepth > 1024) {
            throw invalid_argument("Maximum JSON depth must be between 1 and 1024.");
        }
        maximumJsonDepth_ = maximumJsonDepth;
    }

    // Returns the configured base URL.
    const string& NetworkConfiguration::getBaseUrl() const noexcept {
        return baseUrl_;
    }

    // Returns the default request timeout in milliseconds.
    int64_t NetworkConfiguration::getTimeoutMilliseconds() const noexcept {
        return timeoutMilliseconds_;
    }

    // Returns common interceptor headers.
    const vector<HttpHeader>&
    NetworkConfiguration::getCommonHeaders() const noexcept {
        return commonHeaders_;
    }

    // Returns whether the global interceptor is enabled.
    bool NetworkConfiguration::isInterceptorEnabled() const noexcept {
        return interceptorEnabled_;
    }

    // Returns whether request lifecycle logging is enabled.
    bool NetworkConfiguration::shouldLogRequests() const noexcept {
        return logRequests_;
    }

    // Returns whether response lifecycle logging is enabled.
    bool NetworkConfiguration::shouldLogResponses() const noexcept {
        return logResponses_;
    }

    // Returns whether redirects may be followed.
    bool NetworkConfiguration::shouldFollowRedirects() const noexcept {
        return followRedirects_;
    }

    // Returns the maximum buffered response bytes.
    size_t NetworkConfiguration::getMaximumResponseBytes() const noexcept {
        return maximumResponseBytes_;
    }

    // Returns the maximum parsed JSON nesting depth.
    size_t NetworkConfiguration::getMaximumJsonDepth() const noexcept {
        return maximumJsonDepth_;
    }

}
