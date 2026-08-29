#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "crossa/network/json/JsonValue.h"

namespace crossa::network {

class NetworkPolicy final {
public:
    struct Retry {
        std::size_t maxAttempts = 1;
        std::int64_t initialDelayMilliseconds = 100;
        std::int64_t maximumDelayMilliseconds = 5000;
        double backoffMultiplier = 2.0;
        std::vector<long> retryableStatusCodes{
            408, 425, 429, 500, 502, 503, 504
        };
        bool retryNonIdempotent = false;
    };

    struct Proxy {
        std::string url;
        std::string username;
        std::string password;
    };

    struct Certificate {
        bool verifyPeer = true;
        bool verifyHost = true;
        std::string caInfo;
        std::string clientCertificate;
        std::string clientKey;
        std::string pinnedPublicKey;
    };

    struct Telemetry {
        bool enabled = false;
        bool includeHeaders = false;
        bool includeBody = false;
        std::string serviceName = "crossa";
    };

    struct AuthProvider {
        std::string name;
        std::string tokenUrl;
        std::string accessToken;
        std::string refreshToken;
        std::string clientId;
        std::string clientSecret;
        std::string tokenField = "access_token";
        std::int64_t expiresAtMilliseconds = 0;
    };

    struct MultipartPart {
        std::string name;
        std::optional<std::string> data;
        std::optional<std::string> filePath;
        std::optional<std::string> filename;
        std::optional<std::string> contentType;
    };

    [[nodiscard]] static Retry parseRetry(
        const std::optional<json::JsonValue>& value
    );

    [[nodiscard]] static Proxy parseProxy(
        const std::optional<json::JsonValue>& value
    );

    [[nodiscard]] static Certificate parseCertificate(
        const std::optional<json::JsonValue>& value
    );

    [[nodiscard]] static Telemetry parseTelemetry(
        const std::optional<json::JsonValue>& value
    );

    [[nodiscard]] static std::vector<AuthProvider> parseAuthProviders(
        const std::optional<json::JsonValue>& value
    );

    [[nodiscard]] static std::string parseAuthName(
        const std::optional<json::JsonValue>& value
    );

    [[nodiscard]] static std::vector<MultipartPart> parseMultipart(
        const json::JsonValue& value
    );

private:
    [[nodiscard]] static const json::JsonValue& requireObject(
        const json::JsonValue& value,
        const std::string& name
    );

    [[nodiscard]] static std::string requireString(
        const json::JsonValue& value,
        const std::string& name
    );

    [[nodiscard]] static std::int64_t requireInt(
        const json::JsonValue& value,
        const std::string& name
    );

    [[nodiscard]] static double requireDouble(
        const json::JsonValue& value,
        const std::string& name
    );

    [[nodiscard]] static bool requireBool(
        const json::JsonValue& value,
        const std::string& name
    );

    [[nodiscard]] static std::vector<long> requireStatusCodes(
        const json::JsonValue& value
    );

    [[nodiscard]] static std::optional<std::string> optionalString(
        const json::JsonValue& object,
        const std::string& name
    );
};

}
