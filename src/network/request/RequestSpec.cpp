#include "crossa/network/request/RequestSpec.h"

#include <utility>

using namespace std;

namespace crossa::network::request {

    // Creates one evaluated request specification.
    RequestSpec::RequestSpec(
        HttpMethod method,
        string url,
        vector<HttpHeader> headers,
        vector<HttpHeader> customHeaders,
        QueryParameters queryParameters,
        optional<string> body,
        optional<int64_t> timeoutMilliseconds
    )
        : method_(method),
          url_(std::move(url)),
          headers_(std::move(headers)),
          customHeaders_(std::move(customHeaders)),
          queryParameters_(std::move(queryParameters)),
          body_(std::move(body)),
          timeoutMilliseconds_(timeoutMilliseconds) {}

    // Returns the HTTP method.
    HttpMethod RequestSpec::getMethod() const noexcept {
        return method_;
    }

    // Returns the absolute or relative URL input.
    const string& RequestSpec::getUrl() const noexcept {
        return url_;
    }

    // Returns ordinary request headers.
    const vector<HttpHeader>& RequestSpec::getHeaders() const noexcept {
        return headers_;
    }

    // Returns overriding custom request headers.
    const vector<HttpHeader>& RequestSpec::getCustomHeaders() const noexcept {
        return customHeaders_;
    }

    // Returns ordered query parameters.
    const RequestSpec::QueryParameters&
    RequestSpec::getQueryParameters() const noexcept {
        return queryParameters_;
    }

    // Returns the serialized JSON body or null when absent.
    const optional<string>& RequestSpec::getBody() const noexcept {
        return body_;
    }

    // Returns the request-specific timeout or null when absent.
    const optional<int64_t>&
    RequestSpec::getTimeoutMilliseconds() const noexcept {
        return timeoutMilliseconds_;
    }

}
