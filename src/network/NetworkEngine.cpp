#include "crossa/network/NetworkEngine.h"

#include <chrono>
#include <cmath>
#include <exception>
#include <stdexcept>
#include <thread>
#include <utility>

#include "crossa/network/HttpMethod.h"
#include "crossa/network/json/JsonParser.h"
#include "crossa/network/utils/UrlUtils.h"
#include "crossa/runtime/errors/CrossaException.h"

using namespace std;

namespace crossa::network {

class NetworkEngine::CoalescedRequest final {
public:
    mutex mutex_;
    condition_variable completed;
    bool isCompleted = false;
    optional<response::HttpResponse> response;
    exception_ptr error;
};

NetworkEngine::NetworkEngine(
    const NetworkConfiguration& configuration,
    const crossa::utils::Log& log,
    size_t transportPoolSize
)
    : configuration_(configuration),
      log_(log),
      requestBuilder_(configuration),
      interceptor_(configuration, log),
      transport_(transportPoolSize) {}

response::HttpResponse NetworkEngine::execute(
    const request::RequestSpec& spec,
    const runtime::RequestHandle& requestHandle
) {
    requestHandle.throwIfCancellationRequested();
    request::PreparedRequest request = requestBuilder_.build(spec);
    optional<string> authProvider;
    if (spec.getAuthentication().has_value()) {
        authProvider = NetworkPolicy::parseAuthName(spec.getAuthentication());
    } else if (!configuration_.getDefaultAuthProvider().empty()) {
        authProvider = configuration_.getDefaultAuthProvider();
    }
    applyAuthentication(request, authProvider, requestHandle);

    const bool coalesce = spec.getCoalesce().value_or(
        configuration_.shouldCoalesceRequests()
    ) && !spec.getMultipart().has_value();
    if (!coalesce) {
        return executePrepared(request, authProvider, requestHandle);
    }

    const string key = requestKey(request);
    shared_ptr<CoalescedRequest> operation;
    bool owner = false;
    {
        lock_guard lock(coalescingMutex_);
        const auto found = coalescedRequests_.find(key);
        if (found == coalescedRequests_.end()) {
            operation = make_shared<CoalescedRequest>();
            coalescedRequests_.emplace(key, operation);
            owner = true;
        } else {
            operation = found->second;
        }
    }
    if (!owner) {
        unique_lock lock(operation->mutex_);
        while (!operation->isCompleted) {
            requestHandle.throwIfCancellationRequested();
            operation->completed.wait_for(lock, chrono::milliseconds(25));
        }
        if (operation->error != nullptr) {
            rethrow_exception(operation->error);
        }
        return *operation->response;
    }

    try {
        response::HttpResponse response = executePrepared(
            request,
            authProvider,
            requestHandle
        );
        {
            lock_guard lock(operation->mutex_);
            operation->response = response;
            operation->isCompleted = true;
        }
        operation->completed.notify_all();
        lock_guard lock(coalescingMutex_);
        coalescedRequests_.erase(key);
        return response;
    } catch (...) {
        {
            lock_guard lock(operation->mutex_);
            operation->error = current_exception();
            operation->isCompleted = true;
        }
        operation->completed.notify_all();
        lock_guard lock(coalescingMutex_);
        coalescedRequests_.erase(key);
        throw;
    }
}

response::HttpResponse NetworkEngine::executePrepared(
    request::PreparedRequest& request,
    const optional<string>& authProvider,
    const runtime::RequestHandle& requestHandle
) {
    NetworkPolicy::Retry retry = NetworkPolicy::parseRetry(
        request.getRetryPolicy().has_value()
            ? request.getRetryPolicy()
            : configuration_.getRetryPolicy()
    );
    bool refreshed = false;
    size_t maximumAttempts = retry.maxAttempts;
    for (size_t attempt = 1;
         attempt <= maximumAttempts;
         ++attempt) {
        const auto started = chrono::steady_clock::now();
        try {
            requestHandle.throwIfCancellationRequested();
            interceptor_.beforeRequest(request);
            response::HttpResponse response = transport_.execute(
                request,
                requestHandle
            );
            const auto duration = chrono::duration_cast<chrono::milliseconds>(
                chrono::steady_clock::now() - started
            ).count();
            if (response.getStatusCode() >= 200 &&
                response.getStatusCode() < 300) {
                interceptor_.afterResponse(request, response);
                emitTelemetry("completed", request, &response, attempt, duration, "");
                return response;
            }

            if (response.getStatusCode() == 401 &&
                authProvider.has_value() && !refreshed) {
                const optional<NetworkPolicy::AuthProvider> provider =
                    findAuthProvider(*authProvider);
                if (provider.has_value() &&
                    !provider->refreshToken.empty() &&
                    !provider->tokenUrl.empty()) {
                    (void)refreshAccessToken(*provider, requestHandle);
                    applyAuthentication(request, authProvider, requestHandle);
                    refreshed = true;
                    maximumAttempts = retry.maxAttempts + 1;
                    continue;
                }
            }

            runtime::CrossaException failure(
                runtime::CrossaError::httpStatus(response.getStatusCode())
            );
            if (!isRetryableStatus(response.getStatusCode(), retry) ||
                !isRetryableMethod(request.getMethod(), retry) ||
                attempt == maximumAttempts) {
                interceptor_.afterResponse(request, response);
                emitTelemetry("failed", request, &response, attempt, duration, failure.what());
                throw failure;
            }
            interceptor_.onError(request, failure.what());
            emitTelemetry("retry", request, &response, attempt, duration, failure.what());
        } catch (const runtime::CrossaException& failure) {
            const bool retryableStatus = !failure.getError().getHttpStatus().has_value() ||
                isRetryableStatus(
                    *failure.getError().getHttpStatus(),
                    retry
                );
            if (!failure.getError().isRetryable() || !retryableStatus ||
                !isRetryableMethod(request.getMethod(), retry) ||
                attempt == maximumAttempts) {
                const auto duration = chrono::duration_cast<chrono::milliseconds>(
                    chrono::steady_clock::now() - started
                ).count();
                interceptor_.onError(request, failure.what());
                emitTelemetry("failed", request, nullptr, attempt, duration, failure.what());
                throw;
            }
            interceptor_.onError(request, failure.what());
            const auto duration = chrono::duration_cast<chrono::milliseconds>(
                chrono::steady_clock::now() - started
            ).count();
            emitTelemetry("retry", request, nullptr, attempt, duration, failure.what());
        } catch (const exception& failure) {
            const auto duration = chrono::duration_cast<chrono::milliseconds>(
                chrono::steady_clock::now() - started
            ).count();
            interceptor_.onError(request, failure.what());
            emitTelemetry("failed", request, nullptr, attempt, duration, failure.what());
            throw;
        }
        const double delay = retry.initialDelayMilliseconds *
            pow(retry.backoffMultiplier, static_cast<double>(attempt - 1));
        waitBeforeRetry(
            min(
                retry.maximumDelayMilliseconds,
                static_cast<int64_t>(delay)
            ),
            requestHandle
        );
    }
    throw runtime::CrossaException(
        runtime::CrossaError::runtime("Retry policy exhausted unexpectedly.")
    );
}

optional<NetworkPolicy::AuthProvider> NetworkEngine::findAuthProvider(
    const string& name
) const {
    for (const NetworkPolicy::AuthProvider& provider :
         NetworkPolicy::parseAuthProviders(configuration_.getAuthProviders())) {
        if (provider.name == name) {
            return provider;
        }
    }
    throw runtime_error("Unknown authentication provider '" + name + "'.");
}

string NetworkEngine::getAccessToken(
    const NetworkPolicy::AuthProvider& provider
) {
    const int64_t now = chrono::duration_cast<chrono::milliseconds>(
        chrono::system_clock::now().time_since_epoch()
    ).count();
    if (provider.expiresAtMilliseconds > 0 &&
        provider.expiresAtMilliseconds <= now) {
        return "";
    }
    lock_guard lock(authMutex_);
    const auto found = accessTokens_.find(provider.name);
    if (found != accessTokens_.end()) {
        return found->second;
    }
    accessTokens_.emplace(provider.name, provider.accessToken);
    return provider.accessToken;
}

string NetworkEngine::refreshAccessToken(
    const NetworkPolicy::AuthProvider& provider,
    const runtime::RequestHandle& requestHandle
) {
    string body = "grant_type=refresh_token&refresh_token=" +
        utils::UrlUtils::encodeComponent(provider.refreshToken);
    if (!provider.clientId.empty()) {
        body += "&client_id=" + utils::UrlUtils::encodeComponent(provider.clientId);
    }
    if (!provider.clientSecret.empty()) {
        body += "&client_secret=" + utils::UrlUtils::encodeComponent(provider.clientSecret);
    }
    request::PreparedRequest request(
        HttpMethod::Post,
        provider.tokenUrl,
        vector<HttpHeader>{
            HttpHeader("Content-Type", "application/x-www-form-urlencoded"),
            HttpHeader("Accept", "application/json")
        },
        body,
        configuration_.getTimeoutMilliseconds(),
        configuration_.shouldFollowRedirects(),
        configuration_.getMaximumResponseBytes(),
        configuration_.getMaximumResponseHeaderBytes(),
        configuration_.getMaximumResponseHeaderCount(),
        nullopt,
        nullopt,
        configuration_.getProxy(),
        configuration_.getCertificatePolicy(),
        false,
        false,
        nullopt
    );
    const response::HttpResponse response = transport_.execute(
        request,
        requestHandle
    );
    if (response.getStatusCode() < 200 || response.getStatusCode() >= 300) {
        throw runtime::CrossaException(
            runtime::CrossaError::httpStatus(response.getStatusCode())
        );
    }
    const json::JsonValue tokenResponse = json::JsonParser::parse(
        response.getBody(),
        configuration_.getMaximumResponseBytes(),
        configuration_.getMaximumJsonDepth(),
        &requestHandle
    );
    const json::JsonValue* token = tokenResponse.find(provider.tokenField);
    if (token == nullptr || token->getKind() != json::JsonValueKind::String) {
        throw runtime::CrossaException(
            runtime::CrossaError::runtime(
                "Token refresh response omitted the access token."
            )
        );
    }
    lock_guard lock(authMutex_);
    accessTokens_[provider.name] = token->getString();
    return token->getString();
}

void NetworkEngine::applyAuthentication(
    request::PreparedRequest& request,
    const optional<string>& authProvider,
    const runtime::RequestHandle& requestHandle
) {
    if (!authProvider.has_value() || authProvider->empty()) {
        return;
    }
    const optional<NetworkPolicy::AuthProvider> provider =
        findAuthProvider(*authProvider);
    if (!provider.has_value()) {
        return;
    }
    string token = getAccessToken(*provider);
    if (token.empty() && !provider->refreshToken.empty() &&
        !provider->tokenUrl.empty()) {
        token = refreshAccessToken(*provider, requestHandle);
    }
    if (token.empty()) {
        throw runtime_error(
            "Authentication provider '" + *authProvider +
            "' has no access token."
        );
    }
    request.setHeader(HttpHeader("Authorization", "Bearer " + token), true);
}

string NetworkEngine::requestKey(const request::PreparedRequest& request) {
    string key = string(HttpMethodUtils::toString(request.getMethod())) + "\n" +
        request.getUrl() + "\n";
    for (const HttpHeader& header : request.getHeaders()) {
        key += header.getName() + ":" + header.getValue() + "\n";
    }
    if (request.getBody().has_value()) {
        key += *request.getBody();
    }
    return key;
}

bool NetworkEngine::isRetryableMethod(
    HttpMethod method,
    const NetworkPolicy::Retry& policy
) noexcept {
    if (policy.retryNonIdempotent) {
        return true;
    }
    return method == HttpMethod::Get || method == HttpMethod::Put ||
        method == HttpMethod::Delete || method == HttpMethod::Head ||
        method == HttpMethod::Options || method == HttpMethod::Trace ||
        method == HttpMethod::Connect;
}

bool NetworkEngine::isRetryableStatus(
    long statusCode,
    const NetworkPolicy::Retry& policy
) noexcept {
    for (const long retryable : policy.retryableStatusCodes) {
        if (retryable == statusCode) {
            return true;
        }
    }
    return false;
}

void NetworkEngine::waitBeforeRetry(
    int64_t delayMilliseconds,
    const runtime::RequestHandle& requestHandle
) {
    int64_t remaining = delayMilliseconds;
    while (remaining > 0) {
        requestHandle.throwIfCancellationRequested();
        const int64_t slice = min<int64_t>(remaining, 25);
        this_thread::sleep_for(chrono::milliseconds(slice));
        remaining -= slice;
    }
}

void NetworkEngine::emitTelemetry(
    const string& event,
    const request::PreparedRequest& request,
    const response::HttpResponse* response,
    size_t attempt,
    int64_t durationMilliseconds,
    const string& error
) const {
    const NetworkPolicy::Telemetry telemetry = request.getTelemetry()
        .has_value()
        ? NetworkPolicy::parseTelemetry(request.getTelemetry())
        : NetworkPolicy::Telemetry{};
    if (!telemetry.enabled) {
        return;
    }
    string message = "telemetry event=" + event +
        " service=" + telemetry.serviceName +
        " method=" + string(HttpMethodUtils::toString(request.getMethod())) +
        " url=" + utils::UrlUtils::stripQuery(request.getUrl()) +
        " attempt=" + to_string(attempt) +
        " durationMs=" + to_string(durationMilliseconds) +
        " requestBytes=" + to_string(
            request.getBody().has_value() ? request.getBody()->size() : 0
        );
    if (response != nullptr) {
        const response::TransferMetrics& metrics =
            response->getTransferMetrics();
        message += " status=" + to_string(response->getStatusCode()) +
            " responseBytes=" + to_string(response->getBody().size()) +
            " uploadBytes=" + to_string(metrics.uploadBytes) +
            " downloadBytes=" + to_string(metrics.downloadBytes) +
            " downloadChunks=" + to_string(metrics.downloadChunks) +
            " progressEvents=" + to_string(metrics.progressEvents) +
            " streamed=" + string(metrics.streamed ? "true" : "false");
    }
    if (!error.empty()) {
        message += " error=" + error;
    }
    if (telemetry.includeHeaders) {
        message += " headerCount=" + to_string(request.getHeaders().size());
    }
    if (telemetry.includeBody) {
        message += " bodyIncluded=false";
    }
    log_.debug(message);
}

}
