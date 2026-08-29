#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "crossa/network/HttpHeader.h"

namespace crossa::network::response {

struct TransferMetrics final {
    std::uint64_t uploadTotal = 0;
    std::uint64_t uploadBytes = 0;
    std::uint64_t downloadTotal = 0;
    std::uint64_t downloadBytes = 0;
    std::size_t downloadChunks = 0;
    std::size_t progressEvents = 0;
    bool streamed = false;
};

// Owns one buffered native HTTP response and its metadata.
// getStatusCode(), getHeaders(), and getBody() expose immutable results.
class HttpResponse final {
public:
    // Creates one complete buffered HTTP response.
    HttpResponse(
        long statusCode,
        std::vector<HttpHeader> headers,
        std::string body,
        TransferMetrics metrics = {}
    );

    // Returns the HTTP status code.
    [[nodiscard]] long getStatusCode() const noexcept;

    // Returns ordered response headers.
    [[nodiscard]] const std::vector<HttpHeader>& getHeaders() const noexcept;

    // Returns buffered response body bytes.
    [[nodiscard]] const std::string& getBody() const noexcept;

    [[nodiscard]] const TransferMetrics& getTransferMetrics() const noexcept;

private:
    long statusCode_;
    std::vector<HttpHeader> headers_;
    std::string body_;
    TransferMetrics metrics_;
};

}
