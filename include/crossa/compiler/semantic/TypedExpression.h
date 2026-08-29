#pragma once

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "crossa/compiler/source/SourceLocation.h"
#include "crossa/compiler/types/SemanticType.h"

namespace crossa::compiler::semantic {

// Identifies each expression shape in the typed semantic model.
enum class TypedExpressionKind {
    Identifier,
    IntegerLiteral,
    DecimalLiteral,
    StringLiteral,
    BooleanLiteral,
    Call,
    Unary,
    Binary,
    JsonNumber,
    JsonNull,
    JsonObject,
    JsonArray,
    CrossaRequest
};

// Identifies where a resolved value symbol is owned.
enum class ValueSymbolKind {
    SourceVariable,
    Parameter,
    LocalVariable
};

// Identifies the supported typed unary operation.
enum class TypedUnaryOperator {
    Negate
};

// Identifies the supported typed arithmetic operations.
enum class TypedBinaryOperator {
    Add,
    Subtract,
    Multiply,
    Divide
};

// Distinguishes static string content from resolved symbol reads.
enum class TypedStringSegmentKind {
    Literal,
    Symbol
};

// Stores one pre-resolved string construction segment.
// Symbol segments retain the resolved type and ownership category.
class TypedStringSegment final {
public:
    // Creates one static or symbol-backed string segment.
    TypedStringSegment(
        TypedStringSegmentKind kind,
        std::string value,
        types::SemanticType type,
        std::optional<ValueSymbolKind> symbolKind,
        source::SourceLocation location
    );

    // Returns whether this segment is static text or a symbol read.
    [[nodiscard]] TypedStringSegmentKind getKind() const noexcept;

    // Returns the static text or resolved symbol name.
    [[nodiscard]] const std::string& getValue() const noexcept;

    // Returns the segment value type.
    [[nodiscard]] const types::SemanticType& getType() const noexcept;

    // Returns the resolved symbol category for symbol segments.
    [[nodiscard]] const ValueSymbolKind* getSymbolKind() const noexcept;

    // Returns where this segment begins in source.
    [[nodiscard]] const source::SourceLocation& getLocation() const noexcept;

private:
    TypedStringSegmentKind kind_;
    std::string value_;
    types::SemanticType type_;
    std::optional<ValueSymbolKind> symbolKind_;
    source::SourceLocation location_;
};

// Provides the polymorphic base for all typed semantic expressions.
// Every node carries its resolved result type and source location.
class TypedExpression {
public:
    // Releases a concrete typed expression through the base type.
    virtual ~TypedExpression() = default;

    // Returns the concrete typed expression category.
    [[nodiscard]] TypedExpressionKind getKind() const noexcept;

    // Returns the fully resolved expression result type.
    [[nodiscard]] const types::SemanticType& getType() const noexcept;

    // Returns where this expression begins in source.
    [[nodiscard]] const source::SourceLocation& getLocation() const noexcept;

protected:
    // Creates a typed expression with its shape, type, and location.
    TypedExpression(
        TypedExpressionKind kind,
        types::SemanticType type,
        source::SourceLocation location
    );

private:
    TypedExpressionKind kind_;
    types::SemanticType type_;
    source::SourceLocation location_;
};

// Represents one identifier resolved to a concrete value symbol.
class TypedIdentifierExpression final : public TypedExpression {
public:
    // Creates a typed identifier bound to one symbol category.
    TypedIdentifierExpression(
        std::string name,
        ValueSymbolKind symbolKind,
        types::SemanticType type,
        source::SourceLocation location
    );

    // Returns the resolved symbol name.
    [[nodiscard]] const std::string& getName() const noexcept;

    // Returns the resolved symbol ownership category.
    [[nodiscard]] ValueSymbolKind getSymbolKind() const noexcept;

private:
    std::string name_;
    ValueSymbolKind symbolKind_;
};

// Represents one source integer literal with the resolved Int or Long type.
class TypedIntegerLiteralExpression final : public TypedExpression {
public:
    // Creates a typed integer literal while preserving its source digits.
    TypedIntegerLiteralExpression(
        std::string value,
        types::SemanticType type,
        source::SourceLocation location
    );

    // Returns the source digits of this integer literal.
    [[nodiscard]] const std::string& getValue() const noexcept;

private:
    std::string value_;
};

// Represents one source decimal literal with the resolved Double type.
class TypedDecimalLiteralExpression final : public TypedExpression {
public:
    // Creates a typed decimal literal while preserving its source text.
    TypedDecimalLiteralExpression(
        std::string value,
        source::SourceLocation location
    );

    // Returns the source text of this decimal literal.
    [[nodiscard]] const std::string& getValue() const noexcept;

private:
    std::string value_;
};

// Represents one pre-resolved interpolated string expression.
class TypedStringLiteralExpression final : public TypedExpression {
public:
    // Creates a typed String expression from resolved string segments.
    TypedStringLiteralExpression(
        std::vector<TypedStringSegment> segments,
        source::SourceLocation location
    );

    // Returns the ordered string construction segments.
    [[nodiscard]] const std::vector<TypedStringSegment>&
    getSegments() const noexcept;

private:
    std::vector<TypedStringSegment> segments_;
};

// Represents one typed boolean literal.
class TypedBooleanLiteralExpression final : public TypedExpression {
public:
    // Creates a typed Bool literal.
    TypedBooleanLiteralExpression(
        bool value,
        source::SourceLocation location
    );

    // Returns the parsed boolean value.
    [[nodiscard]] bool getValue() const noexcept;

private:
    bool value_;
};

// Represents one validated function or print builtin call.
class TypedCallExpression final : public TypedExpression {
public:
    // Creates a typed call with validated arguments and result type.
    TypedCallExpression(
        std::string callee,
        bool builtin,
        std::vector<std::unique_ptr<TypedExpression>> arguments,
        types::SemanticType type,
        source::SourceLocation location
    );

    // Returns the resolved callable name.
    [[nodiscard]] const std::string& getCallee() const noexcept;

    // Returns whether the call targets a language builtin.
    [[nodiscard]] bool isBuiltin() const noexcept;

    // Returns the ordered validated call arguments.
    [[nodiscard]] const std::vector<std::unique_ptr<TypedExpression>>&
    getArguments() const noexcept;

private:
    std::string callee_;
    bool builtin_;
    std::vector<std::unique_ptr<TypedExpression>> arguments_;
};

// Represents one validated unary operation and its typed operand.
class TypedUnaryExpression final : public TypedExpression {
public:
    // Creates a typed unary expression.
    TypedUnaryExpression(
        TypedUnaryOperator operation,
        std::unique_ptr<TypedExpression> operand,
        types::SemanticType type,
        source::SourceLocation location
    );

    // Returns the validated unary operator.
    [[nodiscard]] TypedUnaryOperator getOperator() const noexcept;

    // Returns the typed operand expression.
    [[nodiscard]] const TypedExpression& getOperand() const noexcept;

private:
    TypedUnaryOperator operation_;
    std::unique_ptr<TypedExpression> operand_;
};

// Represents one validated arithmetic expression with typed operands.
class TypedBinaryExpression final : public TypedExpression {
public:
    // Creates a typed binary expression.
    TypedBinaryExpression(
        std::unique_ptr<TypedExpression> left,
        TypedBinaryOperator operation,
        std::unique_ptr<TypedExpression> right,
        types::SemanticType type,
        source::SourceLocation location
    );

    // Returns the typed left operand.
    [[nodiscard]] const TypedExpression& getLeft() const noexcept;

    // Returns the validated binary operator.
    [[nodiscard]] TypedBinaryOperator getOperator() const noexcept;

    // Returns the typed right operand.
    [[nodiscard]] const TypedExpression& getRight() const noexcept;

private:
    std::unique_ptr<TypedExpression> left_;
    TypedBinaryOperator operation_;
    std::unique_ptr<TypedExpression> right_;
};

}
