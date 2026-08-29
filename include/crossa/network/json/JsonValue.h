#pragma once

#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace crossa::network::json {

// Identifies every value shape accepted by standard JSON.
enum class JsonValueKind {
    Null,
    Boolean,
    Number,
    String,
    Array,
    Object
};

// Owns one parsed or generated JSON value and exposes typed accessors.
// createObject(), createArray(), and serialize consumers preserve source order.
class JsonValue final {
public:
    using Array = std::vector<JsonValue>;
    using Object = std::vector<std::pair<std::string, JsonValue>>;

    // Creates the JSON null value.
    [[nodiscard]] static JsonValue createNull();

    // Creates a JSON boolean value.
    [[nodiscard]] static JsonValue createBoolean(bool value);

    // Creates a validated JSON number while preserving its exact text.
    [[nodiscard]] static JsonValue createNumber(std::string value);

    // Creates a JSON string value.
    [[nodiscard]] static JsonValue createString(std::string value);

    // Creates a JSON array with ordered values.
    [[nodiscard]] static JsonValue createArray(Array values);

    // Creates a JSON object with ordered fields.
    [[nodiscard]] static JsonValue createObject(Object values);

    // Returns the concrete JSON value category.
    [[nodiscard]] JsonValueKind getKind() const noexcept;

    // Returns the stored boolean and requires a Boolean kind.
    [[nodiscard]] bool getBoolean() const;

    // Returns number text and requires a Number kind.
    [[nodiscard]] const std::string& getNumber() const;

    // Returns string content and requires a String kind.
    [[nodiscard]] const std::string& getString() const;

    // Returns array values and requires an Array kind.
    [[nodiscard]] const Array& getArray() const;

    // Returns object fields and requires an Object kind.
    [[nodiscard]] const Object& getObject() const;

    // Finds one object field by exact key or returns null.
    [[nodiscard]] const JsonValue* find(const std::string& key) const noexcept;

private:
    using Storage = std::variant<
        std::monostate,
        bool,
        std::string,
        Array,
        Object
    >;

    // Creates one JSON value with a matching storage representation.
    JsonValue(JsonValueKind kind, Storage value);

    // Returns whether text follows the complete JSON number grammar.
    [[nodiscard]] static bool isValidNumber(const std::string& value) noexcept;

    JsonValueKind kind_;
    Storage value_;
};

}
