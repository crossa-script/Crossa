#include "crossa/runtime/RuntimeValue.h"

#include <stdexcept>
#include <utility>

#include "crossa/network/json/JsonSerializer.h"
#include "crossa/runtime/objects/NativeList.h"
#include "crossa/runtime/objects/NativeModel.h"

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

    // Creates a runtime Json value.
    RuntimeValue RuntimeValue::createJson(network::json::JsonValue value) {
        return RuntimeValue(RuntimeValueKind::Json, std::move(value));
    }

    // Creates a typed native model value.
    RuntimeValue RuntimeValue::createModel(NativeModel value) {
        return RuntimeValue(
            RuntimeValueKind::Model,
            make_shared<const NativeModel>(std::move(value))
        );
    }

    // Creates a typed native list value.
    RuntimeValue RuntimeValue::createList(NativeList value) {
        return RuntimeValue(
            RuntimeValueKind::List,
            make_shared<const NativeList>(std::move(value))
        );
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

    // Returns the stored Json value and requires a Json kind.
    const network::json::JsonValue& RuntimeValue::getJson() const {
        if (kind_ != RuntimeValueKind::Json) {
            throw runtime_error("Runtime value is not Json.");
        }
        return get<network::json::JsonValue>(value_);
    }

    // Returns the stored native model and requires a Model kind.
    const NativeModel& RuntimeValue::getModel() const {
        if (kind_ != RuntimeValueKind::Model) {
            throw runtime_error("Runtime value is not a native model.");
        }
        return *get<shared_ptr<const NativeModel>>(value_);
    }

    // Returns the stored native list and requires a List kind.
    const NativeList& RuntimeValue::getList() const {
        if (kind_ != RuntimeValueKind::List) {
            throw runtime_error("Runtime value is not a native list.");
        }
        return *get<shared_ptr<const NativeList>>(value_);
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
            case RuntimeValueKind::Json:
                return network::json::JsonSerializer::serialize(getJson());
            case RuntimeValueKind::Model:
            case RuntimeValueKind::List:
                return formatJson();
        }

        return "Unknown";
    }

    // Creates a runtime value from its kind and owned storage.
    RuntimeValue::RuntimeValue(RuntimeValueKind kind, Storage value)
        : kind_(kind), value_(std::move(value)) {}

    // Formats this value as valid JSON for nested native values.
    string RuntimeValue::formatJson() const {
        switch (kind_) {
            case RuntimeValueKind::Unit:
                return "null";
            case RuntimeValueKind::Int:
                return to_string(getInt());
            case RuntimeValueKind::String:
                return network::json::JsonSerializer::serialize(
                    network::json::JsonValue::createString(getString())
                );
            case RuntimeValueKind::Bool:
                return getBool() ? "true" : "false";
            case RuntimeValueKind::Json:
                return network::json::JsonSerializer::serialize(getJson());
            case RuntimeValueKind::Model: {
                string output = "{";
                const NativeModel::Fields& fields = getModel().getFields();
                for (size_t index = 0; index < fields.size(); ++index) {
                    if (index > 0) {
                        output += ",";
                    }
                    output += network::json::JsonSerializer::serialize(
                        network::json::JsonValue::createString(
                            fields[index].first
                        )
                    );
                    output += ":" + fields[index].second.formatJson();
                }
                return output + "}";
            }
            case RuntimeValueKind::List: {
                string output = "[";
                const vector<RuntimeValue>& values = getList().getValues();
                for (size_t index = 0; index < values.size(); ++index) {
                    if (index > 0) {
                        output += ",";
                    }
                    output += values[index].formatJson();
                }
                return output + "]";
            }
        }
        return "null";
    }

}
