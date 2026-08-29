#pragma once

#include <cstddef>
#include <memory>

#include "crossa/network/request/PreparedRequest.h"
#include "crossa/network/response/HttpResponse.h"
#include "crossa/runtime/RequestHandle.h"

namespace crossa::network::transport {

// Owns a bounded pool of reusable libcurl handles for native HTTP execution.
// execute() performs one request while preserving connection reuse across calls.
class CurlTransport final {
public:
    // Initializes libcurl and creates the bounded reusable handle pool.
    explicit CurlTransport(std::size_t poolSize);

    // Releases every reusable handle and the process libcurl state.
    ~CurlTransport();

    CurlTransport(const CurlTransport&) = delete;
    CurlTransport& operator=(const CurlTransport&) = delete;

    // Executes one prepared request and returns its buffered native response.
    [[nodiscard]] response::HttpResponse execute(
        const request::PreparedRequest& request,
        const runtime::RequestHandle& requestHandle
    );

private:
    class Implementation;

    std::unique_ptr<Implementation> implementation_;
};

}
