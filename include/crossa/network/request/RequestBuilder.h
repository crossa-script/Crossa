#pragma once

#include <string>
#include <vector>

#include "crossa/network/NetworkConfiguration.h"
#include "crossa/network/request/PreparedRequest.h"
#include "crossa/network/request/RequestSpec.h"

namespace crossa::network::request {

// Converts evaluated request data into one validated transport request.
// build() applies URL, query, header precedence, body, timeout, and limits.
class RequestBuilder final {
public:
    // Creates a builder over immutable runtime networking configuration.
    explicit RequestBuilder(const NetworkConfiguration& configuration) noexcept;

    // Builds one complete request ready for transport.
    [[nodiscard]] PreparedRequest build(const RequestSpec& spec) const;

private:
    // Appends ordered encoded query parameters to one URL.
    [[nodiscard]] static std::string appendQueryParameters(
        std::string url,
        const RequestSpec::QueryParameters& parameters
    );

    // Validates one header against injection and malformed-name risks.
    static void validateHeader(const HttpHeader& header);

    const NetworkConfiguration& configuration_;
};

}
