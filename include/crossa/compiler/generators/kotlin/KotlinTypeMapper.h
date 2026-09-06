#pragma once

#include <string>

#include "crossa/compiler/types/SemanticType.h"

namespace crossa::compiler::generators::kotlin {

// Maps supported Crossa semantic types onto pure and native-backed Kotlin APIs.
class KotlinTypeMapper final {
public:
    // Returns the Kotlin type syntax for one value-bearing pure Crossa type.
    [[nodiscard]] std::string mapValueType(
        const types::SemanticType& type
    ) const;

    // Returns the public Kotlin type for one native-backed Android result.
    [[nodiscard]] std::string mapNativeApiType(
        const types::SemanticType& type
    ) const;

    // Returns a Kotlin expression that reads one native value as the mapped type.
    [[nodiscard]] std::string nativeValueExpression(
        const types::SemanticType& type,
        const std::string& valueExpression
    ) const;

    // Returns the AsyncAfter mapper closure for one native-backed result type.
    [[nodiscard]] std::string nativeResultMapper(
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
