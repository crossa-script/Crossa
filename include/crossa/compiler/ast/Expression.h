#pragma once

#include <memory>
#include <string>
#include <vector>

#include "crossa/compiler/source/SourceLocation.h"

namespace crossa::compiler::ast {

// Identifies the concrete expression shape stored in the AST.
enum class ExpressionKind {
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
    HttpMethod,
    CrossaRequest
};

// Identifies the arithmetic unary operators supported by V0.
enum class UnaryOperator {
    Negate
};

// Identifies the arithmetic binary operators supported by V0.
enum class BinaryOperator {
    Add,
    Subtract,
    Multiply,
    Divide,
    Equal,
    NotEqual,
    Less,
    LessEqual,
    Greater,
    GreaterEqual
};

// Distinguishes literal text from interpolated identifiers in strings.
enum class StringSegmentKind {
    Literal,
    Identifier
};

// Stores one literal or identifier segment from a Crossa string.
// getKind() and getValue() expose the pre-parsed interpolation plan.
class StringSegment final {
public:
    // Creates one string segment with its semantic category and text.
    StringSegment(
        StringSegmentKind kind,
        std::string value,
        source::SourceLocation location
    );

    // Returns whether this segment is literal text or an identifier.
    [[nodiscard]] StringSegmentKind getKind() const noexcept;

    // Returns the literal text or interpolated identifier name.
    [[nodiscard]] const std::string& getValue() const noexcept;

    // Returns the source location where this segment begins.
    [[nodiscard]] const source::SourceLocation& getLocation() const noexcept;

private:
    StringSegmentKind kind_;
    std::string value_;
    source::SourceLocation location_;
};

// Provides the polymorphic base for every Crossa AST expression.
// getKind() enables deterministic traversal without target-specific behavior.
class Expression {
public:
    // Releases a concrete expression through the base type.
    virtual ~Expression() = default;

    // Returns the concrete expression category.
    [[nodiscard]] ExpressionKind getKind() const noexcept;

    // Returns the source location where this expression begins.
    [[nodiscard]] const source::SourceLocation& getLocation() const noexcept;

protected:
    // Creates an expression with its concrete category.
    Expression(
        ExpressionKind kind,
        source::SourceLocation location
    ) noexcept;

private:
    ExpressionKind kind_;
    source::SourceLocation location_;
};

// Represents a reference to a variable, parameter, or callable name.
// getName() exposes the unresolved identifier for semantic analysis.
class IdentifierExpression final : public Expression {
public:
    // Creates an identifier expression from its source name.
    IdentifierExpression(
        std::string name,
        source::SourceLocation location
    );

    // Returns the unresolved identifier name.
    [[nodiscard]] const std::string& getName() const noexcept;

private:
    std::string name_;
};

// Preserves one base-10 integer literal before type-width decisions.
// getValue() exposes its source digits without platform conversion.
class IntegerLiteralExpression final : public Expression {
public:
    // Creates an integer literal from its source digits.
    IntegerLiteralExpression(
        std::string value,
        source::SourceLocation location
    );

    // Returns the source digits of this integer literal.
    [[nodiscard]] const std::string& getValue() const noexcept;

private:
    std::string value_;
};

// Preserves one decimal literal before runtime conversion.
// getValue() exposes the source number text without platform conversion.
class DecimalLiteralExpression final : public Expression {
public:
    // Creates a decimal literal from its source number text.
    DecimalLiteralExpression(
        std::string value,
        source::SourceLocation location
    );

    // Returns the source text of this decimal literal.
    [[nodiscard]] const std::string& getValue() const noexcept;

private:
    std::string value_;
};

// Represents one string literal and its pre-parsed interpolation segments.
// getSegments() supplies the future semantic interpolation pass.
class StringLiteralExpression final : public Expression {
public:
    // Creates a string expression from parsed literal and identifier segments.
    StringLiteralExpression(
        std::vector<StringSegment> segments,
        source::SourceLocation location
    );

    // Returns the ordered string interpolation segments.
    [[nodiscard]] const std::vector<StringSegment>& getSegments() const noexcept;

private:
    std::vector<StringSegment> segments_;
};

// Represents one true or false literal in the Crossa AST.
// getValue() exposes its parsed boolean value.
class BooleanLiteralExpression final : public Expression {
public:
    // Creates a boolean literal with its parsed value.
    BooleanLiteralExpression(
        bool value,
        source::SourceLocation location
    ) noexcept;

    // Returns the parsed boolean value.
    [[nodiscard]] bool getValue() const noexcept;

private:
    bool value_;
};

// Represents a function or builtin call with ordered arguments.
// getCallee() and getArguments() expose the unresolved call shape.
class CallExpression final : public Expression {
public:
    // Creates a call expression from its callee and arguments.
    CallExpression(
        std::string callee,
        std::vector<std::unique_ptr<Expression>> arguments,
        source::SourceLocation location
    );

    // Returns the unresolved callable name.
    [[nodiscard]] const std::string& getCallee() const noexcept;

    // Returns the ordered call arguments.
    [[nodiscard]] const std::vector<std::unique_ptr<Expression>>&
    getArguments() const noexcept;

private:
    std::string callee_;
    std::vector<std::unique_ptr<Expression>> arguments_;
};

// Represents one unary operation over a nested expression.
// Its accessors expose the operator and owned operand.
class UnaryExpression final : public Expression {
public:
    // Creates a unary expression from an operator and operand.
    UnaryExpression(
        UnaryOperator operation,
        std::unique_ptr<Expression> operand,
        source::SourceLocation location
    );

    // Returns the unary operator.
    [[nodiscard]] UnaryOperator getOperator() const noexcept;

    // Returns the owned operand expression.
    [[nodiscard]] const Expression& getOperand() const noexcept;

private:
    UnaryOperator operation_;
    std::unique_ptr<Expression> operand_;
};

// Represents one arithmetic operation with left and right operands.
// Its accessors expose the operator and both owned expressions.
class BinaryExpression final : public Expression {
public:
    // Creates a binary expression from its operands and operator.
    BinaryExpression(
        std::unique_ptr<Expression> left,
        BinaryOperator operation,
        std::unique_ptr<Expression> right,
        source::SourceLocation location
    );

    // Returns the left operand.
    [[nodiscard]] const Expression& getLeft() const noexcept;

    // Returns the arithmetic operator.
    [[nodiscard]] BinaryOperator getOperator() const noexcept;

    // Returns the right operand.
    [[nodiscard]] const Expression& getRight() const noexcept;

private:
    std::unique_ptr<Expression> left_;
    BinaryOperator operation_;
    std::unique_ptr<Expression> right_;
};

}
