#pragma once

#include <memory>
#include <string>
#include <vector>

#include "crossa/compiler/semantic/TypedExpression.h"

namespace crossa::compiler::semantic {

// Represents one validated JSON decimal or exponent number.
// getValue() preserves exact source text for deterministic lowering.
class TypedJsonNumberExpression final : public TypedExpression {
public:
    // Creates one typed JSON number expression.
    TypedJsonNumberExpression(
        std::string value,
        source::SourceLocation location
    );

    // Returns the exact validated JSON number text.
    [[nodiscard]] const std::string& getValue() const noexcept;

private:
    std::string value_;
};

// Represents JSON null as a Json value without Crossa nullable semantics.
// Its inherited type is always the built-in Json type.
class TypedJsonNullExpression final : public TypedExpression {
public:
    // Creates one typed JSON null expression.
    explicit TypedJsonNullExpression(source::SourceLocation location);
};

// Stores one validated JSON object field and typed value expression.
// getKey() and getValue() preserve deterministic object construction order.
class TypedJsonObjectEntry final {
public:
    // Creates one typed JSON object field.
    TypedJsonObjectEntry(
        std::string key,
        std::unique_ptr<TypedExpression> value,
        source::SourceLocation location
    );

    // Returns the JSON object field key.
    [[nodiscard]] const std::string& getKey() const noexcept;

    // Returns the typed field value expression.
    [[nodiscard]] const TypedExpression& getValue() const noexcept;

    // Returns where the field key begins.
    [[nodiscard]] const source::SourceLocation& getLocation() const noexcept;

private:
    std::string key_;
    std::unique_ptr<TypedExpression> value_;
    source::SourceLocation location_;
};

// Represents one validated JSON object with unique ordered fields.
// getEntries() supplies values for IR lowering.
class TypedJsonObjectExpression final : public TypedExpression {
public:
    // Creates one typed JSON object expression.
    TypedJsonObjectExpression(
        std::vector<TypedJsonObjectEntry> entries,
        source::SourceLocation location
    );

    // Returns the ordered typed JSON fields.
    [[nodiscard]] const std::vector<TypedJsonObjectEntry>&
    getEntries() const noexcept;

private:
    std::vector<TypedJsonObjectEntry> entries_;
};

// Represents one validated JSON array with ordered typed values.
// getValues() supplies array items for IR lowering.
class TypedJsonArrayExpression final : public TypedExpression {
public:
    // Creates one typed JSON array expression.
    TypedJsonArrayExpression(
        std::vector<std::unique_ptr<TypedExpression>> values,
        source::SourceLocation location
    );

    // Returns the ordered typed JSON values.
    [[nodiscard]] const std::vector<std::unique_ptr<TypedExpression>>&
    getValues() const noexcept;

private:
    std::vector<std::unique_ptr<TypedExpression>> values_;
};

}
