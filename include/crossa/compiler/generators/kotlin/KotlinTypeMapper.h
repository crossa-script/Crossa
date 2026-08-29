#pragma once

#include <string>

#include "crossa/compiler/types/SemanticType.h"

namespace crossa::compiler::generators::kotlin {

// Maps supported pure Crossa semantic types to their Kotlin type syntax.
class KotlinTypeMapper final {
public:
    // Returns the Kotlin type syntax for one value-bearing pure Crossa type.
    [[nodiscard]] std::string mapValueType(
        const types::SemanticType& type
    ) const;

    // Returns whether one function result has no Kotlin value.
    [[nodiscard]] bool isUnit(const types::SemanticType& type) const noexcept;

private:
    // Raises a deterministic unsupported-backend diagnostic for one type.
    [[noreturn]] static void failUnsupportedType(
        const types::SemanticType& type
    );
};

}
