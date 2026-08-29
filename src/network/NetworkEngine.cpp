#include "crossa/network/NetworkEngine.h"

#include <stdexcept>
#include <string>

using namespace std;

namespace crossa::network {

// Creates one native network engine with a bounded transport pool.
NetworkEngine::NetworkEngine(
    const NetworkConfiguration& configuration,
    const utils::Log& log,
    size_t transportPoolSize
)
    : requestBuilder_(configuration),
      interceptor_(configuration, log),
      transport_(transportPoolSize) {}

// Builds and executes one request through the shared native pipeline.
response::HttpResponse NetworkEngine::execute(
    const request::RequestSpec& spec
) {
    request::PreparedRequest request = requestBuilder_.build(spec);
    try {
        interceptor_.beforeRequest(request);
        response::HttpResponse response = transport_.execute(request);
        interceptor_.afterResponse(request, response);
        if (response.getStatusCode() < 200 ||
            response.getStatusCode() >= 300) {
            throw runtime_error(
                "HTTP request failed with status " +
                to_string(response.getStatusCode()) + "."
            );
        }
        return response;
    } catch (const exception& error) {
        interceptor_.onError(request, error.what());
        throw;
    }
}

}
