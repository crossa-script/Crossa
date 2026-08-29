#include "crossa/network/utils/UrlUtils.h"

#include <stdexcept>
#include <string_view>

using namespace std;

namespace crossa::network::utils {

    // Returns whether a URL begins with a supported HTTP scheme.
    bool UrlUtils::isAbsoluteHttpUrl(const string& url) noexcept {
        return url.starts_with("http://") || url.starts_with("https://");
    }

    // Joins one relative URL to a configured HTTP base URL.
    string UrlUtils::join(const string& baseUrl, const string& relativeUrl) {
        if (isAbsoluteHttpUrl(relativeUrl)) {
            return relativeUrl;
        }
        if (relativeUrl.find("://") != string::npos) {
            throw invalid_argument(
                "CrossaRequest URL must use HTTP or HTTPS."
            );
        }
        if (!isAbsoluteHttpUrl(baseUrl)) {
            throw invalid_argument(
                "A relative CrossaRequest URL requires an HTTP/HTTPS baseUrl."
            );
        }
        if (relativeUrl.empty()) {
            return baseUrl;
        }
        const bool baseEndsWithSlash = baseUrl.back() == '/';
        const bool pathStartsWithSlash = relativeUrl.front() == '/';
        if (baseEndsWithSlash && pathStartsWithSlash) {
            return baseUrl + relativeUrl.substr(1);
        }
        if (!baseEndsWithSlash && !pathStartsWithSlash) {
            return baseUrl + "/" + relativeUrl;
        }
        return baseUrl + relativeUrl;
    }

    // Percent encodes one path or query component using RFC 3986 unreserved bytes.
    string UrlUtils::encodeComponent(const string& value) {
        constexpr string_view Hex = "0123456789ABCDEF";
        string output;
        output.reserve(value.size());
        for (const unsigned char byte : value) {
            if (isUnreserved(byte)) {
                output.push_back(static_cast<char>(byte));
                continue;
            }
            output.push_back('%');
            output.push_back(Hex[(byte >> 4U) & 0x0FU]);
            output.push_back(Hex[byte & 0x0FU]);
        }
        return output;
    }

    // Removes query content before privacy-aware debug logging.
    string UrlUtils::stripQuery(const string& url) {
        const size_t queryStart = url.find('?');
        return queryStart == string::npos ? url : url.substr(0, queryStart);
    }

    // Returns whether one byte is RFC 3986 unreserved.
    bool UrlUtils::isUnreserved(unsigned char value) noexcept {
        return (value >= 'a' && value <= 'z') ||
               (value >= 'A' && value <= 'Z') ||
               (value >= '0' && value <= '9') ||
               value == '-' || value == '.' || value == '_' || value == '~';
    }

}
