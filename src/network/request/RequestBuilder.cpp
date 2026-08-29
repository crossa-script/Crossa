#include "crossa/network/request/RequestBuilder.h"

#include <stdexcept>
#include <utility>

#include "crossa/network/utils/UrlUtils.h"

using namespace std;

namespace crossa::network::request {

    // Creates a builder over immutable runtime networking configuration.
    RequestBuilder::RequestBuilder(
        const NetworkConfiguration& configuration
    ) noexcept
        : configuration_(configuration) {}

    // Builds one complete request ready for transport.
    PreparedRequest RequestBuilder::build(const RequestSpec& spec) const {
        string url = utils::UrlUtils::join(
            configuration_.getBaseUrl(),
            spec.getUrl()
        );
        url = appendQueryParameters(
            std::move(url),
            spec.getQueryParameters()
        );

        vector<HttpHeader> headers;
        PreparedRequest request(
            spec.getMethod(),
            std::move(url),
            std::move(headers),
            spec.getBody(),
            spec.getTimeoutMilliseconds().value_or(
                configuration_.getTimeoutMilliseconds()
            ),
            configuration_.shouldFollowRedirects(),
            configuration_.getMaximumResponseBytes()
        );
        for (const HttpHeader& header : spec.getHeaders()) {
            validateHeader(header);
            request.setHeader(header, true);
        }
        for (const HttpHeader& header : spec.getCustomHeaders()) {
            validateHeader(header);
            request.setHeader(header, true);
        }
        return request;
    }

    // Appends ordered encoded query parameters to one URL.
    string RequestBuilder::appendQueryParameters(
        string url,
        const RequestSpec::QueryParameters& parameters
    ) {
        const size_t fragmentStart = url.find('#');
        const string fragment = fragmentStart == string::npos
            ? ""
            : url.substr(fragmentStart);
        if (fragmentStart != string::npos) {
            url.erase(fragmentStart);
        }
        bool hasQuery = url.find('?') != string::npos;
        for (const auto& [name, value] : parameters) {
            url.push_back(hasQuery ? '&' : '?');
            hasQuery = true;
            url += utils::UrlUtils::encodeComponent(name);
            url.push_back('=');
            url += utils::UrlUtils::encodeComponent(value);
        }
        return url + fragment;
    }

    // Validates one header against injection and malformed-name risks.
    void RequestBuilder::validateHeader(const HttpHeader& header) {
        if (header.getName().empty() ||
            header.getName().find(':') != string::npos ||
            header.getName().find('\r') != string::npos ||
            header.getName().find('\n') != string::npos) {
            throw invalid_argument("Invalid CrossaRequest header name.");
        }
        if (header.getValue().find('\r') != string::npos ||
            header.getValue().find('\n') != string::npos) {
            throw invalid_argument("Invalid CrossaRequest header value.");
        }
    }

}
