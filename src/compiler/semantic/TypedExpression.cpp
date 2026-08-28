#include "crossa/compiler/semantic/TypedExpression.h"

#include <utility>

using namespace std;

namespace crossa::compiler::semantic {

    // Creates one static or symbol-backed string segment.
    TypedStringSegment::TypedStringSegment(
        TypedStringSegmentKind kind,
        string value,
        types::SemanticType type,
        optional<ValueSymbolKind> symbolKind,
        source::SourceLocation location
    )
        : kind_(kind),
          value_(std::move(value)),
          type_(std::move(type)),
          symbolKind_(symbolKind),
          location_(location) {}

    // Returns whether this segment is static text or a symbol read.
    TypedStringSegmentKind TypedStringSegment::getKind() const noexcept {
        return kind_;
    }

    // Returns the static text or resolved symbol name.
    const string& TypedStringSegment::getValue() const noexcept {
        return value_;
    }

    // Returns the segment value type.
    const types::SemanticType& TypedStringSegment::getType() const noexcept {
        return type_;
    }

    // Returns the resolved symbol category for symbol segments.
    const ValueSymbolKind* TypedStringSegment::getSymbolKind() const noexcept {
        return symbolKind_.has_value() ? &symbolKind_.value() : nullptr;
    }

    // Returns where this segment begins in source.
    const source::SourceLocation&
    TypedStringSegment::getLocation() const noexcept {
        return location_;
    }

    // Creates a typed expression with its shape, type, and location.
    TypedExpression::TypedExpression(
        TypedExpressionKind kind,
        types::SemanticType type,
        source::SourceLocation location
    )
        : kind_(kind), type_(std::move(type)), location_(location) {}

    // Returns the concrete typed expression category.
    TypedExpressionKind TypedExpression::getKind() const noexcept {
        return kind_;
    }

    // Returns the fully resolved expression result type.
    const types::SemanticType& TypedExpression::getType() const noexcept {
        return type_;
    }

    // Returns where this expression begins in source.
    const source::SourceLocation&
    TypedExpression::getLocation() const noexcept {
        return location_;
    }

    // Creates a typed identifier bound to one symbol category.
    TypedIdentifierExpression::TypedIdentifierExpression(
        string name,
        ValueSymbolKind symbolKind,
        types::SemanticType type,
        source::SourceLocation location
    )
        : TypedExpression(
              TypedExpressionKind::Identifier,
              std::move(type),
              location
          ),
          name_(std::move(name)),
          symbolKind_(symbolKind) {}

    // Returns the resolved symbol name.
    const string& TypedIdentifierExpression::getName() const noexcept {
        return name_;
    }

    // Returns the resolved symbol ownership category.
    ValueSymbolKind TypedIdentifierExpression::getSymbolKind() const noexcept {
        return symbolKind_;
    }

    // Creates a typed integer literal while preserving its source digits.
    TypedIntegerLiteralExpression::TypedIntegerLiteralExpression(
        string value,
        source::SourceLocation location
    )
        : TypedExpression(
              TypedExpressionKind::IntegerLiteral,
              types::SemanticType::createInt(),
              location
          ),
          value_(std::move(value)) {}

    // Returns the source digits of this integer literal.
    const string& TypedIntegerLiteralExpression::getValue() const noexcept {
        return value_;
    }

    // Creates a typed String expression from resolved string segments.
    TypedStringLiteralExpression::TypedStringLiteralExpression(
        vector<TypedStringSegment> segments,
        source::SourceLocation location
    )
        : TypedExpression(
              TypedExpressionKind::StringLiteral,
              types::SemanticType::createString(),
              location
          ),
          segments_(std::move(segments)) {}

    // Returns the ordered string construction segments.
    const vector<TypedStringSegment>&
    TypedStringLiteralExpression::getSegments() const noexcept {
        return segments_;
    }

    // Creates a typed Bool literal.
    TypedBooleanLiteralExpression::TypedBooleanLiteralExpression(
        bool value,
        source::SourceLocation location
    )
        : TypedExpression(
              TypedExpressionKind::BooleanLiteral,
              types::SemanticType::createBool(),
              location
          ),
          value_(value) {}

    // Returns the parsed boolean value.
    bool TypedBooleanLiteralExpression::getValue() const noexcept {
        return value_;
    }

    // Creates a typed call with validated arguments and result type.
    TypedCallExpression::TypedCallExpression(
        string callee,
        bool builtin,
        vector<unique_ptr<TypedExpression>> arguments,
        types::SemanticType type,
        source::SourceLocation location
    )
        : TypedExpression(
              TypedExpressionKind::Call,
              std::move(type),
              location
          ),
          callee_(std::move(callee)),
          builtin_(builtin),
          arguments_(std::move(arguments)) {}

    // Returns the resolved callable name.
    const string& TypedCallExpression::getCallee() const noexcept {
        return callee_;
    }

    // Returns whether the call targets a language builtin.
    bool TypedCallExpression::isBuiltin() const noexcept {
        return builtin_;
    }

    // Returns the ordered validated call arguments.
    const vector<unique_ptr<TypedExpression>>&
    TypedCallExpression::getArguments() const noexcept {
        return arguments_;
    }

    // Creates a typed unary expression.
    TypedUnaryExpression::TypedUnaryExpression(
        TypedUnaryOperator operation,
        unique_ptr<TypedExpression> operand,
        types::SemanticType type,
        source::SourceLocation location
    )
        : TypedExpression(
              TypedExpressionKind::Unary,
              std::move(type),
              location
          ),
          operation_(operation),
          operand_(std::move(operand)) {}

    // Returns the validated unary operator.
    TypedUnaryOperator TypedUnaryExpression::getOperator() const noexcept {
        return operation_;
    }

    // Returns the typed operand expression.
    const TypedExpression& TypedUnaryExpression::getOperand() const noexcept {
        return *operand_;
    }

    // Creates a typed binary expression.
    TypedBinaryExpression::TypedBinaryExpression(
        unique_ptr<TypedExpression> left,
        TypedBinaryOperator operation,
        unique_ptr<TypedExpression> right,
        types::SemanticType type,
        source::SourceLocation location
    )
        : TypedExpression(
              TypedExpressionKind::Binary,
              std::move(type),
              location
          ),
          left_(std::move(left)),
          operation_(operation),
          right_(std::move(right)) {}

    // Returns the typed left operand.
    const TypedExpression& TypedBinaryExpression::getLeft() const noexcept {
        return *left_;
    }

    // Returns the validated binary operator.
    TypedBinaryOperator TypedBinaryExpression::getOperator() const noexcept {
        return operation_;
    }

    // Returns the typed right operand.
    const TypedExpression& TypedBinaryExpression::getRight() const noexcept {
        return *right_;
    }

}
