#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <variant>

#include "crossa/network/json/JsonValue.h"

namespace crossa::runtime {

class NativeList;
class NativeModel;

// Identifies every value currently owned by the native runtime.
enum class RuntimeValueKind {
    Unit,
    Int,
    Long,
    Double,
    String,
    Bool,
    Json,
    Model,
    List
};

// Owns one scalar, JSON boundary, typed model, or typed list runtime value.
// Models and lists use immutable native storage shared with future bindings.
class RuntimeValue final {
public:
    // Creates the runtime Unit value.
    static RuntimeValue createUnit();

    // Creates a runtime Int value.
    static RuntimeValue createInt(std::int64_t value);

    // Creates a runtime Long value.
    static RuntimeValue createLong(std::int64_t value);

    // Creates a runtime Double value.
    static RuntimeValue createDouble(double value);

    // Creates a runtime String value.
    static RuntimeValue createString(std::string value);

    // Creates a runtime Bool value.
    static RuntimeValue createBool(bool value);

    // Creates a runtime Json value.
    static RuntimeValue createJson(network::json::JsonValue value);

    // Creates a typed native model value.
    static RuntimeValue createModel(NativeModel value);

    // Creates a typed native list value.
    static RuntimeValue createList(NativeList value);

    // Returns the stored runtime value category.
    [[nodiscard]] RuntimeValueKind getKind() const noexcept;

    // Returns the stored Int value and requires an Int kind.
    [[nodiscard]] std::int64_t getInt() const;

    // Returns the stored Long value and requires a Long kind.
    [[nodiscard]] std::int64_t getLong() const;

    // Returns the stored Double value and requires a Double kind.
    [[nodiscard]] double getDouble() const;

    // Returns the stored String value and requires a String kind.
    [[nodiscard]] const std::string& getString() const;

    // Returns the stored Bool value and requires a Bool kind.
    [[nodiscard]] bool getBool() const;

    // Returns the stored Json value and requires a Json kind.
    [[nodiscard]] const network::json::JsonValue& getJson() const;

    // Returns the stored native model and requires a Model kind.
    [[nodiscard]] const NativeModel& getModel() const;

    // Returns the stored native list and requires a List kind.
    [[nodiscard]] const NativeList& getList() const;

    // Returns a stable human-readable representation for output.
    [[nodiscard]] std::string format() const;

private:
    using Storage = std::variant<
        std::monostate,
        std::int64_t,
        double,
        std::string,
        bool,
        network::json::JsonValue,
        std::shared_ptr<const NativeModel>,
        std::shared_ptr<const NativeList>
    >;

    // Creates a runtime value from its kind and owned storage.
    RuntimeValue(RuntimeValueKind kind, Storage value);

    // Formats this value as valid JSON for nested native values.
    [[nodiscard]] std::string formatJson() const;

    // Formats a Double value with stable compact text.
    [[nodiscard]] static std::string formatDouble(double value);

    RuntimeValueKind kind_;
    Storage value_;
};

}
