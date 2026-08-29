#include "crossa/compiler/generators/kotlin/KotlinTypeMapper.h"

#include <stdexcept>

using namespace std;

namespace crossa::compiler::generators::kotlin {

    // Returns the Kotlin type syntax for one value-bearing pure Crossa type.
    string KotlinTypeMapper::mapValueType(
        const types::SemanticType& type
    ) const {
        switch (type.getKind()) {
            case types::SemanticTypeKind::Int:
                return "Int";
            case types::SemanticTypeKind::Long:
                return "Long";
            case types::SemanticTypeKind::Double:
                return "Double";
            case types::SemanticTypeKind::String:
                return "String";
            case types::SemanticTypeKind::Bool:
                return "Boolean";
            case types::SemanticTypeKind::Unit:
            case types::SemanticTypeKind::Json:
            case types::SemanticTypeKind::Model:
            case types::SemanticTypeKind::List:
                failUnsupportedType(type);
        }

        failUnsupportedType(type);
    }

    // Returns whether one function result has no Kotlin value.
    bool KotlinTypeMapper::isUnit(
        const types::SemanticType& type
    ) const noexcept {
        return type.getKind() == types::SemanticTypeKind::Unit;
    }

    // Raises a deterministic unsupported-backend diagnostic for one type.
    [[noreturn]] void KotlinTypeMapper::failUnsupportedType(
        const types::SemanticType& type
    ) {
        throw runtime_error(
            "Kotlin generation does not support type '" + type.format() + "'."
        );
    }

}
