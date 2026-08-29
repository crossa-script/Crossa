#include "crossa/network/json/JsonValue.h"

#include <stdexcept>
#include <utility>

using namespace std;

namespace crossa::network::json {

    // Creates the JSON null value.
    JsonValue JsonValue::createNull() {
        return JsonValue(JsonValueKind::Null, monostate{});
    }

    // Creates a JSON boolean value.
    JsonValue JsonValue::createBoolean(bool value) {
        return JsonValue(JsonValueKind::Boolean, value);
    }

    // Creates a validated JSON number while preserving its exact text.
    JsonValue JsonValue::createNumber(string value) {
        if (!isValidNumber(value)) {
            throw invalid_argument("Invalid JSON number text.");
        }
        return JsonValue(JsonValueKind::Number, std::move(value));
    }

    // Creates a JSON string value.
    JsonValue JsonValue::createString(string value) {
        return JsonValue(JsonValueKind::String, std::move(value));
    }

    // Creates a JSON array with ordered values.
    JsonValue JsonValue::createArray(Array values) {
        return JsonValue(JsonValueKind::Array, std::move(values));
    }

    // Creates a JSON object with ordered fields.
    JsonValue JsonValue::createObject(Object values) {
        return JsonValue(JsonValueKind::Object, std::move(values));
    }

    // Returns the concrete JSON value category.
    JsonValueKind JsonValue::getKind() const noexcept {
        return kind_;
    }

    // Returns the stored boolean and requires a Boolean kind.
    bool JsonValue::getBoolean() const {
        if (kind_ != JsonValueKind::Boolean) {
            throw runtime_error("JSON value is not Boolean.");
        }
        return get<bool>(value_);
    }

    // Returns number text and requires a Number kind.
    const string& JsonValue::getNumber() const {
        if (kind_ != JsonValueKind::Number) {
            throw runtime_error("JSON value is not Number.");
        }
        return get<string>(value_);
    }

    // Returns string content and requires a String kind.
    const string& JsonValue::getString() const {
        if (kind_ != JsonValueKind::String) {
            throw runtime_error("JSON value is not String.");
        }
        return get<string>(value_);
    }

    // Returns array values and requires an Array kind.
    const JsonValue::Array& JsonValue::getArray() const {
        if (kind_ != JsonValueKind::Array) {
            throw runtime_error("JSON value is not Array.");
        }
        return get<Array>(value_);
    }

    // Returns object fields and requires an Object kind.
    const JsonValue::Object& JsonValue::getObject() const {
        if (kind_ != JsonValueKind::Object) {
            throw runtime_error("JSON value is not Object.");
        }
        return get<Object>(value_);
    }

    // Finds one object field by exact key or returns null.
    const JsonValue* JsonValue::find(const string& key) const noexcept {
        if (kind_ != JsonValueKind::Object) {
            return nullptr;
        }
        for (const auto& [entryKey, entryValue] : get<Object>(value_)) {
            if (entryKey == key) {
                return &entryValue;
            }
        }
        return nullptr;
    }

    // Creates one JSON value with a matching storage representation.
    JsonValue::JsonValue(JsonValueKind kind, Storage value)
        : kind_(kind), value_(std::move(value)) {}

    // Returns whether text follows the complete JSON number grammar.
    bool JsonValue::isValidNumber(const string& value) noexcept {
        size_t current = 0;
        if (current < value.size() && value[current] == '-') {
            ++current;
        }
        if (current >= value.size()) {
            return false;
        }
        if (value[current] == '0') {
            ++current;
            if (current < value.size() &&
                value[current] >= '0' && value[current] <= '9') {
                return false;
            }
        } else {
            if (value[current] < '1' || value[current] > '9') {
                return false;
            }
            while (current < value.size() &&
                   value[current] >= '0' && value[current] <= '9') {
                ++current;
            }
        }
        if (current < value.size() && value[current] == '.') {
            ++current;
            const size_t fractionStart = current;
            while (current < value.size() &&
                   value[current] >= '0' && value[current] <= '9') {
                ++current;
            }
            if (current == fractionStart) {
                return false;
            }
        }
        if (current < value.size() &&
            (value[current] == 'e' || value[current] == 'E')) {
            ++current;
            if (current < value.size() &&
                (value[current] == '+' || value[current] == '-')) {
                ++current;
            }
            const size_t exponentStart = current;
            while (current < value.size() &&
                   value[current] >= '0' && value[current] <= '9') {
                ++current;
            }
            if (current == exponentStart) {
                return false;
            }
        }
        return current == value.size();
    }

}
