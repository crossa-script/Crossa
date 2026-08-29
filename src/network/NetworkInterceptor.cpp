#include "crossa/network/NetworkInterceptor.h"

#include "crossa/network/HttpMethod.h"
#include "crossa/network/utils/UrlUtils.h"

using namespace std;

namespace crossa::network {

    // Creates an interceptor over immutable configuration and logging.
    NetworkInterceptor::NetworkInterceptor(
        const NetworkConfiguration& configuration,
        const crossa::utils::Log& log
    ) noexcept
        : configuration_(configuration), log_(log) {}

    // Appends common headers and reports request lifecycle metadata.
    void NetworkInterceptor::beforeRequest(
        request::PreparedRequest& request
    ) const {
        for (const HttpHeader& header : configuration_.getCommonHeaders()) {
            request.setHeader(header, false);
        }
        if (request.getBody().has_value() &&
            !request.hasHeader("Content-Type")) {
            request.setHeader(
                HttpHeader("Content-Type", "application/json"),
                false
            );
        }
        if (!request.hasHeader("Accept")) {
            request.setHeader(HttpHeader("Accept", "application/json"), false);
        }
        if (!configuration_.isInterceptorEnabled()) {
            return;
        }
        if (configuration_.shouldLogRequests()) {
            string message =
                "Network request started: method=" +
                string(HttpMethodUtils::toString(request.getMethod())) +
                " url=" + utils::UrlUtils::stripQuery(request.getUrl()) +
                " headers=" + to_string(request.getHeaders().size()) +
                " bodyBytes=" + to_string(
                    request.getBody().has_value()
                        ? request.getBody()->size()
                        : 0
                );
            if (configuration_.shouldLogHeaders()) {
                message += " headerValues=" +
                    formatHeaders(request.getHeaders());
            }
            if (configuration_.shouldLogBody() && request.getBody().has_value()) {
                message += " body=" + *request.getBody();
            }
            log_.debug(message);
        }
    }

    // Reports response status and size without logging sensitive bodies.
    void NetworkInterceptor::afterResponse(
        const request::PreparedRequest& request,
        const response::HttpResponse& response
    ) const {
        if (!configuration_.isInterceptorEnabled() ||
            !configuration_.shouldLogResponses()) {
            return;
        }
        string message =
            "Network request completed: url=" +
            utils::UrlUtils::stripQuery(request.getUrl()) +
            " status=" + to_string(response.getStatusCode()) +
            " responseBytes=" + to_string(response.getBody().size());
        if (configuration_.shouldLogHeaders()) {
            message += " headerValues=" + formatHeaders(response.getHeaders());
        }
        if (configuration_.shouldLogBody()) {
            message += " body=" + response.getBody();
        }
        log_.debug(message);
    }

    // Reports a sanitized request failure for the global interception path.
    void NetworkInterceptor::onError(
        const request::PreparedRequest& request,
        const string& message
    ) const {
        if (!configuration_.isInterceptorEnabled()) {
            return;
        }
        log_.debug(
            "Network request failed: url=" +
            utils::UrlUtils::stripQuery(request.getUrl()) +
            " error=" + message
        );
    }

    // Formats headers while excluding configured sensitive names.
    string NetworkInterceptor::formatHeaders(
        const vector<HttpHeader>& headers
    ) const {
        string result = "[";
        bool first = true;
        for (const HttpHeader& header : headers) {
            bool excluded = false;
            for (const string& excludedName :
                 configuration_.getExcludedLogHeaders()) {
                if (headerNamesEqual(header.getName(), excludedName)) {
                    excluded = true;
                    break;
                }
            }
            if (excluded) {
                continue;
            }
            if (!first) {
                result += ", ";
            }
            first = false;
            result += header.getName() + "=" + header.getValue();
        }
        return result + "]";
    }

    // Compares two header names without ASCII case sensitivity.
    bool NetworkInterceptor::headerNamesEqual(
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
