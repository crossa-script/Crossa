#pragma once

#include <string>
#include <vector>

#include "crossa/network/HttpHeader.h"

namespace crossa::network::response {

// Owns one buffered native HTTP response and its metadata.
// getStatusCode(), getHeaders(), and getBody() expose immutable results.
class HttpResponse final {
public:
    // Creates one complete buffered HTTP response.
    HttpResponse(
        long statusCode,
        std::vector<HttpHeader> headers,
        std::string body
    );

    // Returns the HTTP status code.
    [[nodiscard]] long getStatusCode() const noexcept;

    // Returns ordered response headers.
    [[nodiscard]] const std::vector<HttpHeader>& getHeaders() const noexcept;

    // Returns buffered response body bytes.
    [[nodiscard]] const std::string& getBody() const noexcept;

private:
    long statusCode_;
    std::vector<HttpHeader> headers_;
    std::string body_;
};

}
