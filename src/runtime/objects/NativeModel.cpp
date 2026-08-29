#include "crossa/runtime/objects/NativeModel.h"

#include <utility>

using namespace std;

namespace crossa::runtime {

    // Creates one fully decoded model instance.
    NativeModel::NativeModel(string modelName, Fields fields)
        : modelName_(std::move(modelName)), fields_(std::move(fields)) {}

    // Returns the declared Crossa model name.
    const string& NativeModel::getModelName() const noexcept {
        return modelName_;
    }

    // Returns fields in deterministic schema order.
    const NativeModel::Fields& NativeModel::getFields() const noexcept {
        return fields_;
    }

    // Finds a field by exact source name or returns null.
    const RuntimeValue* NativeModel::getField(const string& name) const noexcept {
        for (const Field& field : fields_) {
            if (field.first == name) {
                return &field.second;
            }
        }
        return nullptr;
    }

}
