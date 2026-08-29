#pragma once

#include <cstddef>

#include "crossa/network/NetworkConfiguration.h"
#include "crossa/network/NetworkInterceptor.h"
#include "crossa/network/request/RequestBuilder.h"
#include "crossa/network/request/RequestSpec.h"
#include "crossa/network/response/HttpResponse.h"
#include "crossa/network/transport/CurlTransport.h"
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
        const request::RequestSpec& spec
    );

private:
    request::RequestBuilder requestBuilder_;
    NetworkInterceptor interceptor_;
    transport::CurlTransport transport_;
};

}
