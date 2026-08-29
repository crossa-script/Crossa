#pragma once

#include <string>
#include <vector>

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
    // Formats headers while excluding configured sensitive names.
    [[nodiscard]] std::string formatHeaders(
        const std::vector<HttpHeader>& headers
    ) const;

    // Compares two header names without ASCII case sensitivity.
    [[nodiscard]] static bool headerNamesEqual(
        const std::string& left,
        const std::string& right
    ) noexcept;

    const NetworkConfiguration& configuration_;
    const utils::Log& log_;
};

}
