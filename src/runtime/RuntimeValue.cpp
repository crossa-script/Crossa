#include "crossa/runtime/RuntimeValue.h"

#include <stdexcept>
#include <utility>

using namespace std;

namespace crossa::runtime {

    // Creates the runtime Unit value.
    RuntimeValue RuntimeValue::createUnit() {
        return RuntimeValue(RuntimeValueKind::Unit, monostate{});
    }

    // Creates a runtime Int value.
    RuntimeValue RuntimeValue::createInt(int64_t value) {
        return RuntimeValue(RuntimeValueKind::Int, value);
    }

    // Creates a runtime String value.
    RuntimeValue RuntimeValue::createString(string value) {
        return RuntimeValue(RuntimeValueKind::String, std::move(value));
    }

    // Creates a runtime Bool value.
    RuntimeValue RuntimeValue::createBool(bool value) {
        return RuntimeValue(RuntimeValueKind::Bool, value);
    }

    // Returns the stored runtime value category.
    RuntimeValueKind RuntimeValue::getKind() const noexcept {
        return kind_;
    }

    // Returns the stored Int value and requires an Int kind.
    int64_t RuntimeValue::getInt() const {
        if (kind_ != RuntimeValueKind::Int) {
            throw runtime_error("Runtime value is not Int.");
        }
        return get<int64_t>(value_);
    }

    // Returns the stored String value and requires a String kind.
    const string& RuntimeValue::getString() const {
        if (kind_ != RuntimeValueKind::String) {
            throw runtime_error("Runtime value is not String.");
        }
        return get<string>(value_);
    }

    // Returns the stored Bool value and requires a Bool kind.
    bool RuntimeValue::getBool() const {
        if (kind_ != RuntimeValueKind::Bool) {
            throw runtime_error("Runtime value is not Bool.");
        }
        return get<bool>(value_);
    }

    // Returns a stable human-readable representation for output.
    string RuntimeValue::format() const {
        switch (kind_) {
            case RuntimeValueKind::Unit:
                return "Unit";
            case RuntimeValueKind::Int:
                return to_string(getInt());
            case RuntimeValueKind::String:
                return getString();
            case RuntimeValueKind::Bool:
                return getBool() ? "true" : "false";
        }

        return "Unknown";
    }

    // Creates a runtime value from its kind and owned storage.
    RuntimeValue::RuntimeValue(
        RuntimeValueKind kind,
        variant<monostate, int64_t, string, bool> value
    )
        : kind_(kind), value_(std::move(value)) {}

}
