#include "crossa/network/json/JsonSerializer.h"

#include <string_view>

using namespace std;

namespace crossa::network::json {

    // Serializes one JSON value into compact JSON text.
    string JsonSerializer::serialize(const JsonValue& value) {
        string output;
        appendValue(value, output);
        return output;
    }

    // Appends one JSON value recursively to the output buffer.
    void JsonSerializer::appendValue(const JsonValue& value, string& output) {
        switch (value.getKind()) {
            case JsonValueKind::Null:
                output += "null";
                return;
            case JsonValueKind::Boolean:
                output += value.getBoolean() ? "true" : "false";
                return;
            case JsonValueKind::Number:
                output += value.getNumber();
                return;
            case JsonValueKind::String:
                appendString(value.getString(), output);
                return;
            case JsonValueKind::Array: {
                output.push_back('[');
                bool first = true;
                for (const JsonValue& item : value.getArray()) {
                    if (!first) {
                        output.push_back(',');
                    }
                    first = false;
                    appendValue(item, output);
                }
                output.push_back(']');
                return;
            }
            case JsonValueKind::Object: {
                output.push_back('{');
                bool first = true;
                for (const auto& [key, item] : value.getObject()) {
                    if (!first) {
                        output.push_back(',');
                    }
                    first = false;
                    appendString(key, output);
                    output.push_back(':');
                    appendValue(item, output);
                }
                output.push_back('}');
                return;
            }
        }
    }

    // Appends one escaped JSON string including its quotes.
    void JsonSerializer::appendString(const string& value, string& output) {
        constexpr string_view Hex = "0123456789ABCDEF";
        output.push_back('"');
        for (const unsigned char byte : value) {
            switch (byte) {
                case '"':
                    output += "\\\"";
                    break;
                case '\\':
                    output += "\\\\";
                    break;
                case '\b':
                    output += "\\b";
                    break;
                case '\f':
                    output += "\\f";
                    break;
                case '\n':
                    output += "\\n";
                    break;
                case '\r':
                    output += "\\r";
                    break;
                case '\t':
                    output += "\\t";
                    break;
                default:
                    if (byte < 0x20) {
                        output += "\\u00";
                        output.push_back(Hex[(byte >> 4U) & 0x0FU]);
                        output.push_back(Hex[byte & 0x0FU]);
                    } else {
                        output.push_back(static_cast<char>(byte));
                    }
                    break;
            }
        }
        output.push_back('"');
    }

}
