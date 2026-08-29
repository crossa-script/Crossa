#pragma once

#include <string>
#include <utility>
#include <vector>

#include "crossa/runtime/RuntimeValue.h"

namespace crossa::runtime {

// Owns one immutable typed model instance with schema-ordered native fields.
// getField() and getFields() expose values without a generic JSON DOM.
class NativeModel final {
public:
    using Field = std::pair<std::string, RuntimeValue>;
    using Fields = std::vector<Field>;

    // Creates one fully decoded model instance.
    NativeModel(std::string modelName, Fields fields);

    // Returns the declared Crossa model name.
    [[nodiscard]] const std::string& getModelName() const noexcept;

    // Returns fields in deterministic schema order.
    [[nodiscard]] const Fields& getFields() const noexcept;

    // Finds a field by exact source name or returns null.
    [[nodiscard]] const RuntimeValue* getField(
        const std::string& name
    ) const noexcept;

private:
    std::string modelName_;
    Fields fields_;
};

}
