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
            log_.debug(
                "Network request started: method=" +
                string(HttpMethodUtils::toString(request.getMethod())) +
                " url=" + utils::UrlUtils::stripQuery(request.getUrl()) +
                " headers=" + to_string(request.getHeaders().size()) +
                " bodyBytes=" + to_string(
                    request.getBody().has_value()
                        ? request.getBody()->size()
                        : 0
                )
            );
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
        log_.debug(
            "Network request completed: url=" +
            utils::UrlUtils::stripQuery(request.getUrl()) +
            " status=" + to_string(response.getStatusCode()) +
            " responseBytes=" + to_string(response.getBody().size())
        );
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

}
