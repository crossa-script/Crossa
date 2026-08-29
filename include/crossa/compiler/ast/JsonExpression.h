#pragma once

#include <memory>
#include <string>
#include <vector>

#include "crossa/compiler/ast/Expression.h"

namespace crossa::compiler::ast {

// Represents a decimal or exponent JSON number outside the Crossa Int type.
// getValue() preserves exact validated source text for native serialization.
class JsonNumberExpression final : public Expression {
public:
    // Creates a JSON number expression from exact source text.
    JsonNumberExpression(
        std::string value,
        source::SourceLocation location
    );

    // Returns the exact JSON number source text.
    [[nodiscard]] const std::string& getValue() const noexcept;

private:
    std::string value_;
};

// Represents the JSON null literal without introducing Crossa nullability.
// Its expression kind lets semantic analysis restrict null to Json contexts.
class JsonNullExpression final : public Expression {
public:
    // Creates one JSON null expression.
    explicit JsonNullExpression(source::SourceLocation location) noexcept;
};

// Stores one ordered JSON object field and its source expression value.
// getKey() and getValue() expose syntax for semantic validation.
class JsonObjectEntry final {
public:
    // Creates one JSON object field.
    JsonObjectEntry(
        std::string key,
        std::unique_ptr<Expression> value,
        source::SourceLocation location
    );

    // Returns the decoded JSON object key.
    [[nodiscard]] const std::string& getKey() const noexcept;

    // Returns the source expression used as the field value.
    [[nodiscard]] const Expression& getValue() const noexcept;

    // Returns where the field key begins.
    [[nodiscard]] const source::SourceLocation& getLocation() const noexcept;

private:
    std::string key_;
    std::unique_ptr<Expression> value_;
    source::SourceLocation location_;
};

// Represents an ordered JSON object literal with expression-backed values.
// getEntries() preserves field order for deterministic lowering.
class JsonObjectExpression final : public Expression {
public:
    // Creates a JSON object expression from ordered fields.
    JsonObjectExpression(
        std::vector<JsonObjectEntry> entries,
        source::SourceLocation location
    );

    // Returns the ordered JSON object fields.
    [[nodiscard]] const std::vector<JsonObjectEntry>& getEntries() const noexcept;

private:
    std::vector<JsonObjectEntry> entries_;
};

// Represents an ordered JSON array literal with expression-backed values.
// getValues() supplies each item for semantic validation and lowering.
class JsonArrayExpression final : public Expression {
public:
    // Creates a JSON array expression from ordered values.
    JsonArrayExpression(
        std::vector<std::unique_ptr<Expression>> values,
        source::SourceLocation location
    );

    // Returns the ordered JSON array values.
    [[nodiscard]] const std::vector<std::unique_ptr<Expression>>&
    getValues() const noexcept;

private:
    std::vector<std::unique_ptr<Expression>> values_;
};

}
