#include "crossa/network/response/HttpResponse.h"

#include <utility>

using namespace std;

namespace crossa::network::response {

    // Creates one complete buffered HTTP response.
    HttpResponse::HttpResponse(
        long statusCode,
        vector<HttpHeader> headers,
        string body
    )
        : statusCode_(statusCode),
          headers_(std::move(headers)),
          body_(std::move(body)) {}

    // Returns the HTTP status code.
    long HttpResponse::getStatusCode() const noexcept {
        return statusCode_;
    }

    // Returns ordered response headers.
    const vector<HttpHeader>& HttpResponse::getHeaders() const noexcept {
        return headers_;
    }

    // Returns buffered response body bytes.
    const string& HttpResponse::getBody() const noexcept {
        return body_;
    }

}
