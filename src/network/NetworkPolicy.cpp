#include "crossa/network/NetworkPolicy.h"

#include <charconv>
#include <stdexcept>
#include <utility>

using namespace std;

namespace crossa::network {

NetworkPolicy::Retry NetworkPolicy::parseRetry(
    const optional<json::JsonValue>& value
) {
    Retry result;
    if (!value.has_value()) {
        return result;
    }
    const json::JsonValue& object = requireObject(*value, "retryPolicy");
    for (const auto& [name, option] : object.getObject()) {
        if (name == "maxAttempts") {
            const int64_t parsed = requireInt(option, name);
            if (parsed < 1 || parsed > 10) {
                throw invalid_argument("retryPolicy maxAttempts must be 1..10.");
            }
            result.maxAttempts = static_cast<size_t>(parsed);
        } else if (name == "initialDelay") {
            result.initialDelayMilliseconds = requireInt(option, name);
        } else if (name == "maxDelay") {
            result.maximumDelayMilliseconds = requireInt(option, name);
        } else if (name == "backoffMultiplier") {
            result.backoffMultiplier = requireDouble(option, name);
        } else if (name == "retryStatusCodes") {
            result.retryableStatusCodes = requireStatusCodes(option);
        } else if (name == "retryNonIdempotent") {
            result.retryNonIdempotent = requireBool(option, name);
        } else {
            throw invalid_argument("Unknown retryPolicy option '" + name + "'.");
        }
    }
    if (result.initialDelayMilliseconds < 0 ||
        result.maximumDelayMilliseconds < result.initialDelayMilliseconds ||
        result.backoffMultiplier < 1.0) {
        throw invalid_argument("Invalid retryPolicy delay configuration.");
    }
    return result;
}

NetworkPolicy::Proxy NetworkPolicy::parseProxy(
    const optional<json::JsonValue>& value
) {
    Proxy result;
    if (!value.has_value()) {
        return result;
    }
    const json::JsonValue& object = requireObject(*value, "proxy");
    for (const auto& [name, option] : object.getObject()) {
        if (name == "url") {
            result.url = requireString(option, name);
        } else if (name == "username") {
            result.username = requireString(option, name);
        } else if (name == "password") {
            result.password = requireString(option, name);
        } else {
            throw invalid_argument("Unknown proxy option '" + name + "'.");
        }
    }
    return result;
}

NetworkPolicy::Certificate NetworkPolicy::parseCertificate(
    const optional<json::JsonValue>& value
) {
    Certificate result;
    if (!value.has_value()) {
        return result;
    }
    const json::JsonValue& object = requireObject(*value, "certificatePolicy");
    for (const auto& [name, option] : object.getObject()) {
        if (name == "verifyPeer") {
            result.verifyPeer = requireBool(option, name);
        } else if (name == "verifyHost") {
            result.verifyHost = requireBool(option, name);
        } else if (name == "caInfo") {
            result.caInfo = requireString(option, name);
        } else if (name == "clientCertificate") {
            result.clientCertificate = requireString(option, name);
        } else if (name == "clientKey") {
            result.clientKey = requireString(option, name);
        } else if (name == "pinnedPublicKey") {
            result.pinnedPublicKey = requireString(option, name);
        } else {
            throw invalid_argument(
                "Unknown certificatePolicy option '" + name + "'."
            );
        }
    }
    if (!result.verifyPeer || !result.verifyHost) {
        throw invalid_argument(
            "certificatePolicy cannot disable peer or host verification."
        );
    }
    return result;
}

NetworkPolicy::Telemetry NetworkPolicy::parseTelemetry(
    const optional<json::JsonValue>& value
) {
    Telemetry result;
    if (!value.has_value()) {
        return result;
    }
    const json::JsonValue& object = requireObject(*value, "telemetry");
    for (const auto& [name, option] : object.getObject()) {
        if (name == "enabled") {
            result.enabled = requireBool(option, name);
        } else if (name == "includeHeaders") {
            result.includeHeaders = requireBool(option, name);
        } else if (name == "includeBody") {
            result.includeBody = requireBool(option, name);
        } else if (name == "serviceName") {
            result.serviceName = requireString(option, name);
        } else {
            throw invalid_argument("Unknown telemetry option '" + name + "'.");
        }
    }
    return result;
}

vector<NetworkPolicy::AuthProvider> NetworkPolicy::parseAuthProviders(
    const optional<json::JsonValue>& value
) {
    vector<AuthProvider> result;
    if (!value.has_value()) {
        return result;
    }
    const json::JsonValue& object = requireObject(*value, "authProviders");
    for (const auto& [providerName, providerValue] : object.getObject()) {
        const json::JsonValue& provider = requireObject(
            providerValue,
            "authProviders." + providerName
        );
        AuthProvider item;
        item.name = providerName;
        for (const auto& [name, option] : provider.getObject()) {
            if (name == "tokenUrl") {
                item.tokenUrl = requireString(option, name);
            } else if (name == "accessToken") {
                item.accessToken = requireString(option, name);
            } else if (name == "refreshToken") {
                item.refreshToken = requireString(option, name);
            } else if (name == "clientId") {
                item.clientId = requireString(option, name);
            } else if (name == "clientSecret") {
                item.clientSecret = requireString(option, name);
            } else if (name == "tokenField") {
                item.tokenField = requireString(option, name);
            } else if (name == "expiresAt") {
                item.expiresAtMilliseconds = requireInt(option, name);
            } else {
                throw invalid_argument(
                    "Unknown auth provider option '" + name + "'."
                );
            }
        }
        result.push_back(std::move(item));
    }
    return result;
}

string NetworkPolicy::parseAuthName(const optional<json::JsonValue>& value) {
    if (!value.has_value()) {
        return "";
    }
    if (value->getKind() == json::JsonValueKind::String) {
        return value->getString();
    }
    const json::JsonValue& object = requireObject(*value, "auth");
    const json::JsonValue* provider = object.find("provider");
    if (provider == nullptr) {
        throw invalid_argument("auth requires a provider.");
    }
    return requireString(*provider, "provider");
}

vector<NetworkPolicy::MultipartPart> NetworkPolicy::parseMultipart(
    const json::JsonValue& value
) {
    const json::JsonValue& object = requireObject(value, "multipart");
    const json::JsonValue* parts = object.find("parts");
    if (parts == nullptr || parts->getKind() != json::JsonValueKind::Array) {
        throw invalid_argument("multipart requires a parts array.");
    }
    vector<MultipartPart> result;
    for (const json::JsonValue& partValue : parts->getArray()) {
        const json::JsonValue& part = requireObject(partValue, "multipart part");
        const json::JsonValue* name = part.find("name");
        if (name == nullptr) {
            throw invalid_argument("multipart part requires name.");
        }
        MultipartPart item;
        item.name = requireString(*name, "name");
        item.data = optionalString(part, "data");
        item.filePath = optionalString(part, "filePath");
        item.filename = optionalString(part, "filename");
        item.contentType = optionalString(part, "contentType");
        if (!item.data.has_value() && !item.filePath.has_value()) {
            throw invalid_argument("multipart part requires data or filePath.");
        }
        if (item.data.has_value() && item.filePath.has_value()) {
            throw invalid_argument("multipart part cannot contain data and filePath.");
        }
        result.push_back(std::move(item));
    }
    return result;
}

const json::JsonValue& NetworkPolicy::requireObject(
    const json::JsonValue& value,
    const string& name
) {
    if (value.getKind() != json::JsonValueKind::Object) {
        throw invalid_argument(name + " must be a JSON object.");
    }
    return value;
}

string NetworkPolicy::requireString(
    const json::JsonValue& value,
    const string& name
) {
    if (value.getKind() != json::JsonValueKind::String) {
        throw invalid_argument(name + " must be a String.");
    }
    return value.getString();
}

int64_t NetworkPolicy::requireInt(
    const json::JsonValue& value,
    const string& name
) {
    if (value.getKind() != json::JsonValueKind::Number) {
        throw invalid_argument(name + " must be an Int.");
    }
    int64_t result = 0;
    const string& text = value.getNumber();
    const auto parsed = from_chars(
        text.data(),
        text.data() + text.size(),
        result
    );
    if (parsed.ec != errc{} || parsed.ptr != text.data() + text.size()) {
        throw invalid_argument(name + " must be an Int.");
    }
    return result;
}

double NetworkPolicy::requireDouble(
    const json::JsonValue& value,
    const string& name
) {
    if (value.getKind() != json::JsonValueKind::Number) {
        throw invalid_argument(name + " must be a Number.");
    }
    try {
        return stod(value.getNumber());
    } catch (...) {
        throw invalid_argument(name + " must be a Number.");
    }
}

bool NetworkPolicy::requireBool(
    const json::JsonValue& value,
    const string& name
) {
    if (value.getKind() != json::JsonValueKind::Boolean) {
        throw invalid_argument(name + " must be a Bool.");
    }
    return value.getBoolean();
}

vector<long> NetworkPolicy::requireStatusCodes(const json::JsonValue& value) {
    if (value.getKind() != json::JsonValueKind::Array) {
        throw invalid_argument("retryStatusCodes must be an array.");
    }
    vector<long> result;
    for (const json::JsonValue& item : value.getArray()) {
        const int64_t status = requireInt(item, "retryStatusCodes item");
        if (status < 100 || status > 599) {
            throw invalid_argument("retryStatusCodes items must be HTTP codes.");
        }
        result.push_back(static_cast<long>(status));
    }
    return result;
}

optional<string> NetworkPolicy::optionalString(
    const json::JsonValue& object,
    const string& name
) {
    const json::JsonValue* value = object.find(name);
    return value == nullptr
        ? nullopt
        : optional<string>(requireString(*value, name));
}

}
