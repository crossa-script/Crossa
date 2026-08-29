#pragma once

#include <string>

#include "crossa/network/json/JsonValue.h"

namespace crossa::network::json {

// Serializes owned JSON values into deterministic compact UTF-8 text.
// serialize() escapes strings and preserves object and array order.
class JsonSerializer final {
public:
    // Serializes one JSON value into compact JSON text.
    [[nodiscard]] static std::string serialize(const JsonValue& value);

private:
    // Appends one JSON value recursively to the output buffer.
    static void appendValue(const JsonValue& value, std::string& output);

    // Appends one escaped JSON string including its quotes.
    static void appendString(const std::string& value, std::string& output);
};

}
