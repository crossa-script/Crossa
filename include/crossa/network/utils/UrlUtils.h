#pragma once

#include <string>

namespace crossa::network::utils {

// Provides stateless HTTP URL joining, encoding, and privacy-safe formatting.
// isAbsoluteHttpUrl(), join(), encodeComponent(), and stripQuery() own URL rules.
class UrlUtils final {
public:
    // Returns whether a URL begins with a supported HTTP scheme.
    [[nodiscard]] static bool isAbsoluteHttpUrl(
        const std::string& url
    ) noexcept;

    // Joins one relative URL to a configured HTTP base URL.
    [[nodiscard]] static std::string join(
        const std::string& baseUrl,
        const std::string& relativeUrl
    );

    // Percent encodes one path or query component using RFC 3986 unreserved bytes.
    [[nodiscard]] static std::string encodeComponent(
        const std::string& value
    );

    // Removes query content before privacy-aware debug logging.
    [[nodiscard]] static std::string stripQuery(const std::string& url);

private:
    // Returns whether one byte is RFC 3986 unreserved.
    [[nodiscard]] static bool isUnreserved(unsigned char value) noexcept;
};

}
