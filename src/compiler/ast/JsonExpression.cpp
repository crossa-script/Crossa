#include "crossa/compiler/ast/JsonExpression.h"

#include <utility>

using namespace std;

namespace crossa::compiler::ast {

    // Creates a JSON number expression from exact source text.
    JsonNumberExpression::JsonNumberExpression(
        string value,
        source::SourceLocation location
    )
        : Expression(ExpressionKind::JsonNumber, location),
          value_(std::move(value)) {}

    // Returns the exact JSON number source text.
    const string& JsonNumberExpression::getValue() const noexcept {
        return value_;
    }

    // Creates one JSON null expression.
    JsonNullExpression::JsonNullExpression(
        source::SourceLocation location
    ) noexcept
        : Expression(ExpressionKind::JsonNull, location) {}

    // Creates one JSON object field.
    JsonObjectEntry::JsonObjectEntry(
        string key,
        unique_ptr<Expression> value,
        source::SourceLocation location
    )
        : key_(std::move(key)),
          value_(std::move(value)),
          location_(location) {}

    // Returns the decoded JSON object key.
    const string& JsonObjectEntry::getKey() const noexcept {
        return key_;
    }

    // Returns the source expression used as the field value.
    const Expression& JsonObjectEntry::getValue() const noexcept {
        return *value_;
    }

    // Returns where the field key begins.
    const source::SourceLocation& JsonObjectEntry::getLocation() const noexcept {
        return location_;
    }

    // Creates a JSON object expression from ordered fields.
    JsonObjectExpression::JsonObjectExpression(
        vector<JsonObjectEntry> entries,
        source::SourceLocation location
    )
        : Expression(ExpressionKind::JsonObject, location),
          entries_(std::move(entries)) {}

    // Returns the ordered JSON object fields.
    const vector<JsonObjectEntry>&
    JsonObjectExpression::getEntries() const noexcept {
        return entries_;
    }

    // Creates a JSON array expression from ordered values.
    JsonArrayExpression::JsonArrayExpression(
        vector<unique_ptr<Expression>> values,
        source::SourceLocation location
    )
        : Expression(ExpressionKind::JsonArray, location),
          values_(std::move(values)) {}

    // Returns the ordered JSON array values.
    const vector<unique_ptr<Expression>>&
    JsonArrayExpression::getValues() const noexcept {
        return values_;
    }

}
