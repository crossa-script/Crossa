#pragma once

#include <cstddef>
#include <vector>

#include "crossa/runtime/RuntimeValue.h"

namespace crossa::runtime {

// Owns one immutable typed Crossa list of native runtime values.
// getValues() and get() provide indexed access without JSON materialization.
class NativeList final {
public:
    // Creates one fully decoded typed native list.
    explicit NativeList(std::vector<RuntimeValue> values);

    // Returns the number of native elements.
    [[nodiscard]] std::size_t getSize() const noexcept;

    // Returns all native elements in response order.
    [[nodiscard]] const std::vector<RuntimeValue>& getValues() const noexcept;

    // Returns one element and validates its index.
    [[nodiscard]] const RuntimeValue& get(std::size_t index) const;

private:
    std::vector<RuntimeValue> values_;
};

}
