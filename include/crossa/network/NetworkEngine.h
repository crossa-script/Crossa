#pragma once

#include <cstddef>
#include <condition_variable>
#include <cstdint>
#include <exception>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>

#include "crossa/network/NetworkConfiguration.h"
#include "crossa/network/NetworkInterceptor.h"
#include "crossa/network/NetworkPolicy.h"
#include "crossa/network/request/RequestBuilder.h"
#include "crossa/network/request/RequestSpec.h"
#include "crossa/network/response/HttpResponse.h"
#include "crossa/network/transport/CurlTransport.h"
#include "crossa/runtime/RequestHandle.h"
#include "crossa/utils/Log.h"

namespace crossa::network {

// Coordinates request construction, global interception, and native transport.
// execute() guarantees one interceptor terminal event for every prepared request.
class NetworkEngine final {
public:
    // Creates one native network engine with a bounded transport pool.
    NetworkEngine(
        const NetworkConfiguration& configuration,
        const utils::Log& log,
        std::size_t transportPoolSize
    );

    // Builds and executes one request through the shared native pipeline.
    [[nodiscard]] response::HttpResponse execute(
        const request::RequestSpec& spec,
        const runtime::RequestHandle& requestHandle
    );

private:
    class CoalescedRequest;

    [[nodiscard]] response::HttpResponse executePrepared(
        request::PreparedRequest& request,
        const std::optional<std::string>& authProvider,
        const runtime::RequestHandle& requestHandle
    );

    [[nodiscard]] std::optional<NetworkPolicy::AuthProvider>
    findAuthProvider(const std::string& name) const;

    [[nodiscard]] std::string getAccessToken(
        const NetworkPolicy::AuthProvider& provider
    );

    [[nodiscard]] std::string refreshAccessToken(
        const NetworkPolicy::AuthProvider& provider,
        const runtime::RequestHandle& requestHandle
    );

    void applyAuthentication(
        request::PreparedRequest& request,
        const std::optional<std::string>& authProvider,
        const runtime::RequestHandle& requestHandle
    );

    [[nodiscard]] static std::string requestKey(
        const request::PreparedRequest& request
    );

    [[nodiscard]] static bool isRetryableMethod(
        HttpMethod method,
        const NetworkPolicy::Retry& policy
    ) noexcept;

    [[nodiscard]] static bool isRetryableStatus(
        long statusCode,
        const NetworkPolicy::Retry& policy
    ) noexcept;

    static void waitBeforeRetry(
        std::int64_t delayMilliseconds,
        const runtime::RequestHandle& requestHandle
    );

    void emitTelemetry(
        const std::string& event,
        const request::PreparedRequest& request,
        const response::HttpResponse* response,
        std::size_t attempt,
        std::int64_t durationMilliseconds,
        const std::string& error
    ) const;

    const NetworkConfiguration& configuration_;
    const utils::Log& log_;
    request::RequestBuilder requestBuilder_;
    NetworkInterceptor interceptor_;
    transport::CurlTransport transport_;
    mutable std::mutex authMutex_;
    std::unordered_map<std::string, std::string> accessTokens_;
    mutable std::mutex coalescingMutex_;
    std::unordered_map<std::string, std::shared_ptr<CoalescedRequest>>
        coalescedRequests_;
};

}
