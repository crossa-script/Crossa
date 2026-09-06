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
            case types::SemanticTypeKind::Model:
                return type.getModelName();
            case types::SemanticTypeKind::List:
                if (type.getElementType() == nullptr) {
                    failUnsupportedType(type);
                }
                return "List<" + mapValueType(*type.getElementType()) + ">";
            case types::SemanticTypeKind::Unit:
            case types::SemanticTypeKind::Json:
                failUnsupportedType(type);
        }

        failUnsupportedType(type);
    }

    // Returns the public Kotlin type for one native-backed Android result.
    string KotlinTypeMapper::mapNativeApiType(
        const types::SemanticType& type
    ) const {
        switch (type.getKind()) {
            case types::SemanticTypeKind::Unit:
                return "Unit";
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
            case types::SemanticTypeKind::Model:
                return type.getModelName();
            case types::SemanticTypeKind::List:
                if (type.getElementType() == nullptr) {
                    failUnsupportedType(type);
                }
                return "CrossaNativeList<" +
                    mapNativeApiType(*type.getElementType()) + ">";
            case types::SemanticTypeKind::Json:
                return "CrossaJson";
        }
        failUnsupportedType(type);
    }

    // Returns a Kotlin expression that reads one native value as the mapped type.
    string KotlinTypeMapper::nativeValueExpression(
        const types::SemanticType& type,
        const string& valueExpression
    ) const {
        switch (type.getKind()) {
            case types::SemanticTypeKind::Int:
                return valueExpression + ".intValue()";
            case types::SemanticTypeKind::Long:
                return valueExpression + ".longValue()";
            case types::SemanticTypeKind::Double:
                return valueExpression + ".doubleValue()";
            case types::SemanticTypeKind::String:
                return valueExpression + ".stringValue()";
            case types::SemanticTypeKind::Bool:
                return valueExpression + ".booleanValue()";
            case types::SemanticTypeKind::Model:
                return type.getModelName() + "(" + valueExpression + ")";
            case types::SemanticTypeKind::List:
                if (type.getElementType() == nullptr) {
                    failUnsupportedType(type);
                }
                return "CrossaNativeList(" + valueExpression +
                    ") { item -> " +
                    nativeValueExpression(*type.getElementType(), "item") +
                    " }";
            case types::SemanticTypeKind::Json:
                return "CrossaJson(" + valueExpression + ")";
            case types::SemanticTypeKind::Unit:
                failUnsupportedType(type);
        }
        failUnsupportedType(type);
    }

    // Returns the AsyncAfter mapper closure for one native-backed result type.
    string KotlinTypeMapper::nativeResultMapper(
        const types::SemanticType& type
    ) const {
        switch (type.getKind()) {
            case types::SemanticTypeKind::Unit:
                return "{ result -> result.close() }";
            case types::SemanticTypeKind::Int:
                return "{ result -> result.intValue().also { result.close() } }";
            case types::SemanticTypeKind::Long:
                return "{ result -> result.longValue().also { result.close() } }";
            case types::SemanticTypeKind::Double:
                return "{ result -> result.doubleValue().also { result.close() } }";
            case types::SemanticTypeKind::String:
                return "{ result -> result.stringValue().also { result.close() } }";
            case types::SemanticTypeKind::Bool:
                return "{ result -> result.booleanValue().also { result.close() } }";
            case types::SemanticTypeKind::Model:
            case types::SemanticTypeKind::List:
            case types::SemanticTypeKind::Json:
                return "{ result -> " +
                    nativeValueExpression(type, "result.rootValue()") + " }";
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
