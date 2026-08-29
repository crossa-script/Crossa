#pragma once

#include <cstdint>
#include <string>
#include <variant>

#include "crossa/network/json/JsonValue.h"

namespace crossa::runtime {

// Identifies the primitive values currently executable by the native runtime.
enum class RuntimeValueKind {
    Unit,
    Int,
    String,
    Bool,
    Json
};

// Owns one runtime value produced while interpreting Crossa IR.
// The first execution slice intentionally supports scalar values only.
class RuntimeValue final {
public:
    // Creates the runtime Unit value.
    static RuntimeValue createUnit();

    // Creates a runtime Int value.
    static RuntimeValue createInt(std::int64_t value);

    // Creates a runtime String value.
    static RuntimeValue createString(std::string value);

    // Creates a runtime Bool value.
    static RuntimeValue createBool(bool value);

    // Creates a runtime Json value.
    static RuntimeValue createJson(network::json::JsonValue value);

    // Returns the stored runtime value category.
    [[nodiscard]] RuntimeValueKind getKind() const noexcept;

    // Returns the stored Int value and requires an Int kind.
    [[nodiscard]] std::int64_t getInt() const;

    // Returns the stored String value and requires a String kind.
    [[nodiscard]] const std::string& getString() const;

    // Returns the stored Bool value and requires a Bool kind.
    [[nodiscard]] bool getBool() const;

    // Returns the stored Json value and requires a Json kind.
    [[nodiscard]] const network::json::JsonValue& getJson() const;

    // Returns a stable human-readable representation for output.
    [[nodiscard]] std::string format() const;

private:
    // Creates a runtime value from its kind and owned storage.
    RuntimeValue(
        RuntimeValueKind kind,
        std::variant<
            std::monostate,
            std::int64_t,
            std::string,
            bool,
            network::json::JsonValue
        > value
    );

    RuntimeValueKind kind_;
    std::variant<
        std::monostate,
        std::int64_t,
        std::string,
        bool,
        network::json::JsonValue
    > value_;
};

}
