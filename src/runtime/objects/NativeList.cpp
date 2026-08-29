#include "crossa/runtime/objects/NativeList.h"

#include <stdexcept>
#include <utility>

using namespace std;

namespace crossa::runtime {

    // Creates one fully decoded typed native list.
    NativeList::NativeList(vector<RuntimeValue> values)
        : values_(std::move(values)) {}

    // Returns the number of native elements.
    size_t NativeList::getSize() const noexcept {
        return values_.size();
    }

    // Returns all native elements in response order.
    const vector<RuntimeValue>& NativeList::getValues() const noexcept {
        return values_;
    }

    // Returns one element and validates its index.
    const RuntimeValue& NativeList::get(size_t index) const {
        if (index >= values_.size()) {
            throw out_of_range("Native list index is out of bounds.");
        }
        return values_[index];
    }

}
