#include "crossa/compiler/ir/IrExpression.h"

#include <utility>

using namespace std;

namespace crossa::compiler::ir {

    // Creates one literal or symbol segment for a string-build instruction.
    IrStringSegment::IrStringSegment(
        IrStringSegmentKind kind,
        string value,
        optional<IrSymbolKind> symbolKind,
        source::SourceLocation location
    )
        : kind_(kind),
          value_(std::move(value)),
          symbolKind_(symbolKind),
          location_(location) {}

    // Returns whether this segment is literal text or a symbol read.
    IrStringSegmentKind IrStringSegment::getKind() const noexcept {
        return kind_;
    }

    // Returns literal text or the resolved symbol name.
    const string& IrStringSegment::getValue() const noexcept {
        return value_;
    }

    // Returns the symbol owner for symbol segments.
    const IrSymbolKind* IrStringSegment::getSymbolKind() const noexcept {
        return symbolKind_.has_value() ? &symbolKind_.value() : nullptr;
    }

    // Returns the source location of this segment.
    const source::SourceLocation& IrStringSegment::getLocation() const noexcept {
        return location_;
    }

    // Creates an IR expression with its category, type, and location.
    IrExpression::IrExpression(
        IrExpressionKind kind,
        types::SemanticType type,
        source::SourceLocation location
    )
        : kind_(kind), type_(std::move(type)), location_(location) {}

    // Returns the concrete IR expression category.
    IrExpressionKind IrExpression::getKind() const noexcept {
        return kind_;
    }

    // Returns the resolved result type of this instruction.
    const types::SemanticType& IrExpression::getType() const noexcept {
        return type_;
    }

    // Returns where this instruction originated in source.
    const source::SourceLocation& IrExpression::getLocation() const noexcept {
        return location_;
    }

    // Creates a symbol-read instruction.
    IrReadSymbolExpression::IrReadSymbolExpression(
        string name,
        IrSymbolKind symbolKind,
        types::SemanticType type,
        source::SourceLocation location
    )
        : IrExpression(
              IrExpressionKind::ReadSymbol,
              std::move(type),
              location
          ),
          name_(std::move(name)),
          symbolKind_(symbolKind) {}

    // Returns the symbol name.
    const string& IrReadSymbolExpression::getName() const noexcept {
        return name_;
    }

    // Returns the symbol owner category.
    IrSymbolKind IrReadSymbolExpression::getSymbolKind() const noexcept {
        return symbolKind_;
    }

    // Creates an integer constant while preserving source digits.
    IrIntegerConstantExpression::IrIntegerConstantExpression(
        string value,
        types::SemanticType type,
        source::SourceLocation location
    )
        : IrExpression(
              IrExpressionKind::IntegerConstant,
              std::move(type),
              location
          ),
          value_(std::move(value)) {}

    // Returns the source integer digits.
    const string& IrIntegerConstantExpression::getValue() const noexcept {
        return value_;
    }

    // Creates a Double constant while preserving source text.
    IrDoubleConstantExpression::IrDoubleConstantExpression(
        string value,
        source::SourceLocation location
    )
        : IrExpression(
              IrExpressionKind::DoubleConstant,
              types::SemanticType::createDouble(),
              location
          ),
          value_(std::move(value)) {}

    // Returns the source decimal text.
    const string& IrDoubleConstantExpression::getValue() const noexcept {
        return value_;
    }

    // Creates a string-build instruction from pre-resolved segments.
    IrStringBuildExpression::IrStringBuildExpression(
        vector<IrStringSegment> segments,
        source::SourceLocation location
    )
        : IrExpression(
              IrExpressionKind::StringBuild,
              types::SemanticType::createString(),
              location
          ),
          segments_(std::move(segments)) {}

    // Returns the ordered string-build segments.
    const vector<IrStringSegment>&
    IrStringBuildExpression::getSegments() const noexcept {
        return segments_;
    }

    // Creates a boolean constant instruction.
    IrBooleanConstantExpression::IrBooleanConstantExpression(
        bool value,
        source::SourceLocation location
    )
        : IrExpression(
              IrExpressionKind::BooleanConstant,
              types::SemanticType::createBool(),
              location
          ),
          value_(value) {}

    // Returns the boolean constant value.
    bool IrBooleanConstantExpression::getValue() const noexcept {
        return value_;
    }

    // Creates a call instruction with lowered argument expressions.
    IrCallExpression::IrCallExpression(
        string callee,
        bool builtin,
        vector<unique_ptr<IrExpression>> arguments,
        types::SemanticType type,
        source::SourceLocation location
    )
        : IrExpression(
              IrExpressionKind::Call,
              std::move(type),
              location
          ),
          callee_(std::move(callee)),
          builtin_(builtin),
          arguments_(std::move(arguments)) {}

    // Returns the callable name.
    const string& IrCallExpression::getCallee() const noexcept {
        return callee_;
    }

    // Returns whether this instruction calls a Crossa builtin.
    bool IrCallExpression::isBuiltin() const noexcept {
        return builtin_;
    }

    // Returns the ordered lowered arguments.
    const vector<unique_ptr<IrExpression>>&
    IrCallExpression::getArguments() const noexcept {
        return arguments_;
    }

    // Creates a unary instruction with its lowered operand.
    IrUnaryExpression::IrUnaryExpression(
        IrArithmeticOperator operation,
        unique_ptr<IrExpression> operand,
        types::SemanticType type,
        source::SourceLocation location
    )
        : IrExpression(
              IrExpressionKind::Unary,
              std::move(type),
              location
          ),
          operation_(operation),
          operand_(std::move(operand)) {}

    // Returns the unary operation.
    IrArithmeticOperator IrUnaryExpression::getOperator() const noexcept {
        return operation_;
    }

    // Returns the lowered operand instruction.
    const IrExpression& IrUnaryExpression::getOperand() const noexcept {
        return *operand_;
    }

    // Creates a binary instruction with lowered operands.
    IrBinaryExpression::IrBinaryExpression(
        unique_ptr<IrExpression> left,
        IrArithmeticOperator operation,
        unique_ptr<IrExpression> right,
        types::SemanticType type,
        source::SourceLocation location
    )
        : IrExpression(
              IrExpressionKind::Binary,
              std::move(type),
              location
          ),
          left_(std::move(left)),
          operation_(operation),
          right_(std::move(right)) {}

    // Returns the lowered left operand.
    const IrExpression& IrBinaryExpression::getLeft() const noexcept {
        return *left_;
    }

    // Returns the binary operation.
    IrArithmeticOperator IrBinaryExpression::getOperator() const noexcept {
        return operation_;
    }

    // Returns the lowered right operand.
    const IrExpression& IrBinaryExpression::getRight() const noexcept {
        return *right_;
    }

}
