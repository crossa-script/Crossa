#include "crossa/compiler/semantic/TypedJsonExpression.h"

#include <utility>

using namespace std;

namespace crossa::compiler::semantic {

    // Creates one typed JSON number expression.
    TypedJsonNumberExpression::TypedJsonNumberExpression(
        string value,
        source::SourceLocation location
    )
        : TypedExpression(
              TypedExpressionKind::JsonNumber,
              types::SemanticType::createJson(),
              location
          ),
          value_(std::move(value)) {}

    // Returns the exact validated JSON number text.
    const string& TypedJsonNumberExpression::getValue() const noexcept {
        return value_;
    }

    // Creates one typed JSON null expression.
    TypedJsonNullExpression::TypedJsonNullExpression(
        source::SourceLocation location
    )
        : TypedExpression(
              TypedExpressionKind::JsonNull,
              types::SemanticType::createJson(),
              location
          ) {}

    // Creates one typed JSON object field.
    TypedJsonObjectEntry::TypedJsonObjectEntry(
        string key,
        unique_ptr<TypedExpression> value,
        source::SourceLocation location
    )
        : key_(std::move(key)),
          value_(std::move(value)),
          location_(location) {}

    // Returns the JSON object field key.
    const string& TypedJsonObjectEntry::getKey() const noexcept {
        return key_;
    }

    // Returns the typed field value expression.
    const TypedExpression& TypedJsonObjectEntry::getValue() const noexcept {
        return *value_;
    }

    // Returns where the field key begins.
    const source::SourceLocation&
    TypedJsonObjectEntry::getLocation() const noexcept {
        return location_;
    }

    // Creates one typed JSON object expression.
    TypedJsonObjectExpression::TypedJsonObjectExpression(
        vector<TypedJsonObjectEntry> entries,
        source::SourceLocation location
    )
        : TypedExpression(
              TypedExpressionKind::JsonObject,
              types::SemanticType::createJson(),
              location
          ),
          entries_(std::move(entries)) {}

    // Returns the ordered typed JSON fields.
    const vector<TypedJsonObjectEntry>&
    TypedJsonObjectExpression::getEntries() const noexcept {
        return entries_;
    }

    // Creates one typed JSON array expression.
    TypedJsonArrayExpression::TypedJsonArrayExpression(
        vector<unique_ptr<TypedExpression>> values,
        source::SourceLocation location
    )
        : TypedExpression(
              TypedExpressionKind::JsonArray,
              types::SemanticType::createJson(),
              location
          ),
          values_(std::move(values)) {}

    // Returns the ordered typed JSON values.
    const vector<unique_ptr<TypedExpression>>&
    TypedJsonArrayExpression::getValues() const noexcept {
        return values_;
    }

}
