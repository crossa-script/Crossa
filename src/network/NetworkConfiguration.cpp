#include "crossa/network/NetworkConfiguration.h"

#include <stdexcept>
#include <string>
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
          logHeaders_(false),
          logBody_(false),
          followRedirects_(true),
          uploadProgress_(false),
          downloadStreaming_(false),
          requestCoalescing_(false),
          maximumResponseBytes_(8U * 1024U * 1024U),
          maximumJsonDepth_(128),
          maximumResponseHeaderBytes_(1024U * 1024U),
          maximumResponseHeaderCount_(256) {}

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

    // Enables or disables request and response header logging.
    void NetworkConfiguration::setLogHeaders(bool enabled) noexcept {
        logHeaders_ = enabled;
    }

    // Enables or disables request and response body logging.
    void NetworkConfiguration::setLogBody(bool enabled) noexcept {
        logBody_ = enabled;
    }

    // Replaces header names that must never appear in logs.
    void NetworkConfiguration::setExcludedLogHeaders(vector<string> headers) {
        for (const string& header : headers) {
            if (header.empty() || header.find(':') != string::npos ||
                header.find('\r') != string::npos ||
                header.find('\n') != string::npos) {
                throw invalid_argument("Invalid excluded log header name.");
            }
        }
        excludedLogHeaders_ = std::move(headers);
    }

    // Enables or disables bounded HTTP redirect following.
    void NetworkConfiguration::setFollowRedirects(bool enabled) noexcept {
        followRedirects_ = enabled;
    }

    void NetworkConfiguration::setRetryPolicy(json::JsonValue policy) {
        retryPolicy_ = std::move(policy);
    }

    void NetworkConfiguration::setAuthProviders(json::JsonValue providers) {
        authProviders_ = std::move(providers);
    }

    void NetworkConfiguration::setDefaultAuthProvider(string provider) {
        defaultAuthProvider_ = std::move(provider);
    }

    void NetworkConfiguration::setUploadProgress(bool enabled) noexcept {
        uploadProgress_ = enabled;
    }

    void NetworkConfiguration::setDownloadStreaming(bool enabled) noexcept {
        downloadStreaming_ = enabled;
    }

    void NetworkConfiguration::setRequestCoalescing(bool enabled) noexcept {
        requestCoalescing_ = enabled;
    }

    void NetworkConfiguration::setProxy(json::JsonValue proxy) {
        proxy_ = std::move(proxy);
    }

    void NetworkConfiguration::setCertificatePolicy(json::JsonValue policy) {
        certificatePolicy_ = std::move(policy);
    }

    void NetworkConfiguration::setTelemetry(json::JsonValue telemetry) {
        telemetry_ = std::move(telemetry);
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

    void NetworkConfiguration::setMaximumResponseHeaderBytes(
        size_t maximumResponseHeaderBytes
    ) {
        if (maximumResponseHeaderBytes == 0 ||
            maximumResponseHeaderBytes > 16U * 1024U * 1024U) {
            throw invalid_argument(
                "Maximum response header bytes must be between 1 and 16777216."
            );
        }
        maximumResponseHeaderBytes_ = maximumResponseHeaderBytes;
    }

    void NetworkConfiguration::setMaximumResponseHeaderCount(
        size_t maximumResponseHeaderCount
    ) {
        if (maximumResponseHeaderCount == 0 ||
            maximumResponseHeaderCount > 4096U) {
            throw invalid_argument(
                "Maximum response header count must be between 1 and 4096."
            );
        }
        maximumResponseHeaderCount_ = maximumResponseHeaderCount;
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

    // Returns whether request and response headers may be logged.
    bool NetworkConfiguration::shouldLogHeaders() const noexcept {
        return logHeaders_;
    }

    // Returns whether request and response bodies may be logged.
    bool NetworkConfiguration::shouldLogBody() const noexcept {
        return logBody_;
    }

    // Returns case-insensitive header names excluded from logs.
    const vector<string>&
    NetworkConfiguration::getExcludedLogHeaders() const noexcept {
        return excludedLogHeaders_;
    }

    // Returns whether redirects may be followed.
    bool NetworkConfiguration::shouldFollowRedirects() const noexcept {
        return followRedirects_;
    }

    const optional<json::JsonValue>&
    NetworkConfiguration::getRetryPolicy() const noexcept {
        return retryPolicy_;
    }

    const optional<json::JsonValue>&
    NetworkConfiguration::getAuthProviders() const noexcept {
        return authProviders_;
    }

    const string& NetworkConfiguration::getDefaultAuthProvider() const noexcept {
        return defaultAuthProvider_;
    }

    bool NetworkConfiguration::shouldReportUploadProgress() const noexcept {
        return uploadProgress_;
    }

    bool NetworkConfiguration::shouldStreamDownloads() const noexcept {
        return downloadStreaming_;
    }

    bool NetworkConfiguration::shouldCoalesceRequests() const noexcept {
        return requestCoalescing_;
    }

    const optional<json::JsonValue>&
    NetworkConfiguration::getProxy() const noexcept {
        return proxy_;
    }

    const optional<json::JsonValue>&
    NetworkConfiguration::getCertificatePolicy() const noexcept {
        return certificatePolicy_;
    }

    const optional<json::JsonValue>&
    NetworkConfiguration::getTelemetry() const noexcept {
        return telemetry_;
    }

    // Returns the maximum buffered response bytes.
    size_t NetworkConfiguration::getMaximumResponseBytes() const noexcept {
        return maximumResponseBytes_;
    }

    // Returns the maximum parsed JSON nesting depth.
    size_t NetworkConfiguration::getMaximumJsonDepth() const noexcept {
        return maximumJsonDepth_;
    }

    size_t NetworkConfiguration::getMaximumResponseHeaderBytes() const noexcept {
        return maximumResponseHeaderBytes_;
    }

    size_t NetworkConfiguration::getMaximumResponseHeaderCount() const noexcept {
        return maximumResponseHeaderCount_;
    }

}
