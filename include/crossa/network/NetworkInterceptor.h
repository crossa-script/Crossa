#pragma once

#include <string>

#include "crossa/network/NetworkConfiguration.h"
#include "crossa/network/request/PreparedRequest.h"
#include "crossa/network/response/HttpResponse.h"
#include "crossa/utils/Log.h"

namespace crossa::network {

// Applies common request metadata and observes every network terminal event.
// beforeRequest(), afterResponse(), and onError form the global interceptor.
class NetworkInterceptor final {
public:
    // Creates an interceptor over immutable configuration and logging.
    NetworkInterceptor(
        const NetworkConfiguration& configuration,
        const utils::Log& log
    ) noexcept;

    // Appends common headers and reports request lifecycle metadata.
    void beforeRequest(request::PreparedRequest& request) const;

    // Reports response status and size without logging sensitive bodies.
    void afterResponse(
        const request::PreparedRequest& request,
        const response::HttpResponse& response
    ) const;

    // Reports a sanitized request failure for the global interception path.
    void onError(
        const request::PreparedRequest& request,
        const std::string& message
    ) const;

private:
    const NetworkConfiguration& configuration_;
    const utils::Log& log_;
};

}
