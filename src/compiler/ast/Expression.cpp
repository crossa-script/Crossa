#include "crossa/compiler/ast/Expression.h"

#include <utility>

using namespace std;

namespace crossa::compiler::ast {

    // Creates one string segment with its semantic category and text.
    StringSegment::StringSegment(
        StringSegmentKind kind,
        string value,
        source::SourceLocation location
    )
        : kind_(kind), value_(std::move(value)), location_(location) {}

    // Returns whether this segment is literal text or an identifier.
    StringSegmentKind StringSegment::getKind() const noexcept {
        return kind_;
    }

    // Returns the literal text or interpolated identifier name.
    const string& StringSegment::getValue() const noexcept {
        return value_;
    }

    // Returns the source location where this segment begins.
    const source::SourceLocation& StringSegment::getLocation() const noexcept {
        return location_;
    }

    // Creates an expression with its concrete category.
    Expression::Expression(
        ExpressionKind kind,
        source::SourceLocation location
    ) noexcept
        : kind_(kind), location_(location) {}

    // Returns the concrete expression category.
    ExpressionKind Expression::getKind() const noexcept {
        return kind_;
    }

    // Returns the source location where this expression begins.
    const source::SourceLocation& Expression::getLocation() const noexcept {
        return location_;
    }

    // Creates an identifier expression from its source name.
    IdentifierExpression::IdentifierExpression(
        string name,
        source::SourceLocation location
    )
        : Expression(ExpressionKind::Identifier, location),
          name_(std::move(name)) {}

    // Returns the unresolved identifier name.
    const string& IdentifierExpression::getName() const noexcept {
        return name_;
    }

    // Creates an integer literal from its source digits.
    IntegerLiteralExpression::IntegerLiteralExpression(
        string value,
        source::SourceLocation location
    )
        : Expression(ExpressionKind::IntegerLiteral, location),
          value_(std::move(value)) {}

    // Returns the source digits of this integer literal.
    const string& IntegerLiteralExpression::getValue() const noexcept {
        return value_;
    }

    // Creates a string expression from parsed literal and identifier segments.
    StringLiteralExpression::StringLiteralExpression(
        vector<StringSegment> segments,
        source::SourceLocation location
    )
        : Expression(ExpressionKind::StringLiteral, location),
          segments_(std::move(segments)) {}

    // Returns the ordered string interpolation segments.
    const vector<StringSegment>&
    StringLiteralExpression::getSegments() const noexcept {
        return segments_;
    }

    // Creates a boolean literal with its parsed value.
    BooleanLiteralExpression::BooleanLiteralExpression(
        bool value,
        source::SourceLocation location
    ) noexcept
        : Expression(ExpressionKind::BooleanLiteral, location), value_(value) {}

    // Returns the parsed boolean value.
    bool BooleanLiteralExpression::getValue() const noexcept {
        return value_;
    }

    // Creates a call expression from its callee and arguments.
    CallExpression::CallExpression(
        string callee,
        vector<unique_ptr<Expression>> arguments,
        source::SourceLocation location
    )
        : Expression(ExpressionKind::Call, location),
          callee_(std::move(callee)),
          arguments_(std::move(arguments)) {}

    // Returns the unresolved callable name.
    const string& CallExpression::getCallee() const noexcept {
        return callee_;
    }

    // Returns the ordered call arguments.
    const vector<unique_ptr<Expression>>&
    CallExpression::getArguments() const noexcept {
        return arguments_;
    }

    // Creates a unary expression from an operator and operand.
    UnaryExpression::UnaryExpression(
        UnaryOperator operation,
        unique_ptr<Expression> operand,
        source::SourceLocation location
    )
        : Expression(ExpressionKind::Unary, location),
          operation_(operation),
          operand_(std::move(operand)) {}

    // Returns the unary operator.
    UnaryOperator UnaryExpression::getOperator() const noexcept {
        return operation_;
    }

    // Returns the owned operand expression.
    const Expression& UnaryExpression::getOperand() const noexcept {
        return *operand_;
    }

    // Creates a binary expression from its operands and operator.
    BinaryExpression::BinaryExpression(
        unique_ptr<Expression> left,
        BinaryOperator operation,
        unique_ptr<Expression> right,
        source::SourceLocation location
    )
        : Expression(ExpressionKind::Binary, location),
          left_(std::move(left)),
          operation_(operation),
          right_(std::move(right)) {}

    // Returns the left operand.
    const Expression& BinaryExpression::getLeft() const noexcept {
        return *left_;
    }

    // Returns the arithmetic operator.
    BinaryOperator BinaryExpression::getOperator() const noexcept {
        return operation_;
    }

    // Returns the right operand.
    const Expression& BinaryExpression::getRight() const noexcept {
        return *right_;
    }

}
