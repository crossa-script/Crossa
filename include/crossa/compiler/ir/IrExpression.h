#pragma once

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "crossa/compiler/source/SourceLocation.h"
#include "crossa/compiler/types/SemanticType.h"

namespace crossa::compiler::ir {

// Identifies the expression instructions currently supported by Crossa IR.
enum class IrExpressionKind {
    ReadSymbol,
    IntegerConstant,
    StringBuild,
    BooleanConstant,
    Call,
    Unary,
    Binary,
    JsonNumber,
    JsonNull,
    JsonObject,
    JsonArray,
    CrossaRequest
};

// Identifies the arithmetic operations represented by Crossa IR.
enum class IrArithmeticOperator {
    Add,
    Subtract,
    Multiply,
    Divide,
    Negate
};

// Identifies the owner of a symbol read in an IR expression.
enum class IrSymbolKind {
    SourceVariable,
    Parameter,
    LocalVariable
};

// Distinguishes static string text from a pre-resolved symbol segment.
enum class IrStringSegmentKind {
    Literal,
    Symbol
};

// Stores one compile-time string-build segment.
class IrStringSegment final {
public:
    // Creates one literal or symbol segment for a string-build instruction.
    IrStringSegment(
        IrStringSegmentKind kind,
        std::string value,
        std::optional<IrSymbolKind> symbolKind,
        source::SourceLocation location
    );

    // Returns whether this segment is literal text or a symbol read.
    [[nodiscard]] IrStringSegmentKind getKind() const noexcept;

    // Returns literal text or the resolved symbol name.
    [[nodiscard]] const std::string& getValue() const noexcept;

    // Returns the symbol owner for symbol segments.
    [[nodiscard]] const IrSymbolKind* getSymbolKind() const noexcept;

    // Returns the source location of this segment.
    [[nodiscard]] const source::SourceLocation& getLocation() const noexcept;

private:
    IrStringSegmentKind kind_;
    std::string value_;
    std::optional<IrSymbolKind> symbolKind_;
    source::SourceLocation location_;
};

// Provides the polymorphic base for platform-neutral IR expressions.
// Each instruction carries its resolved result type and source location.
class IrExpression {
public:
    // Releases a concrete IR expression through the base type.
    virtual ~IrExpression() = default;

    // Returns the concrete IR expression category.
    [[nodiscard]] IrExpressionKind getKind() const noexcept;

    // Returns the resolved result type of this instruction.
    [[nodiscard]] const types::SemanticType& getType() const noexcept;

    // Returns where this instruction originated in source.
    [[nodiscard]] const source::SourceLocation& getLocation() const noexcept;

protected:
    // Creates an IR expression with its category, type, and location.
    IrExpression(
        IrExpressionKind kind,
        types::SemanticType type,
        source::SourceLocation location
    );

private:
    IrExpressionKind kind_;
    types::SemanticType type_;
    source::SourceLocation location_;
};

// Represents an IR read from a resolved source, parameter, or local symbol.
class IrReadSymbolExpression final : public IrExpression {
public:
    // Creates a symbol-read instruction.
    IrReadSymbolExpression(
        std::string name,
        IrSymbolKind symbolKind,
        types::SemanticType type,
        source::SourceLocation location
    );

    // Returns the symbol name.
    [[nodiscard]] const std::string& getName() const noexcept;

    // Returns the symbol owner category.
    [[nodiscard]] IrSymbolKind getSymbolKind() const noexcept;

private:
    std::string name_;
    IrSymbolKind symbolKind_;
};

// Represents an integer constant instruction.
class IrIntegerConstantExpression final : public IrExpression {
public:
    // Creates an integer constant while preserving source digits.
    IrIntegerConstantExpression(
        std::string value,
        source::SourceLocation location
    );

    // Returns the source integer digits.
    [[nodiscard]] const std::string& getValue() const noexcept;

private:
    std::string value_;
};

// Represents a string construction plan with no runtime source parsing.
class IrStringBuildExpression final : public IrExpression {
public:
    // Creates a string-build instruction from pre-resolved segments.
    IrStringBuildExpression(
        std::vector<IrStringSegment> segments,
        source::SourceLocation location
    );

    // Returns the ordered string-build segments.
    [[nodiscard]] const std::vector<IrStringSegment>&
    getSegments() const noexcept;

private:
    std::vector<IrStringSegment> segments_;
};

// Represents a boolean constant instruction.
class IrBooleanConstantExpression final : public IrExpression {
public:
    // Creates a boolean constant instruction.
    IrBooleanConstantExpression(
        bool value,
        source::SourceLocation location
    );

    // Returns the boolean constant value.
    [[nodiscard]] bool getValue() const noexcept;

private:
    bool value_;
};

// Represents a validated function or builtin call instruction.
class IrCallExpression final : public IrExpression {
public:
    // Creates a call instruction with lowered argument expressions.
    IrCallExpression(
        std::string callee,
        bool builtin,
        std::vector<std::unique_ptr<IrExpression>> arguments,
        types::SemanticType type,
        source::SourceLocation location
    );

    // Returns the callable name.
    [[nodiscard]] const std::string& getCallee() const noexcept;

    // Returns whether this instruction calls a Crossa builtin.
    [[nodiscard]] bool isBuiltin() const noexcept;

    // Returns the ordered lowered arguments.
    [[nodiscard]] const std::vector<std::unique_ptr<IrExpression>>&
    getArguments() const noexcept;

private:
    std::string callee_;
    bool builtin_;
    std::vector<std::unique_ptr<IrExpression>> arguments_;
};

// Represents a lowered unary arithmetic instruction.
class IrUnaryExpression final : public IrExpression {
public:
    // Creates a unary instruction with its lowered operand.
    IrUnaryExpression(
        IrArithmeticOperator operation,
        std::unique_ptr<IrExpression> operand,
        types::SemanticType type,
        source::SourceLocation location
    );

    // Returns the unary operation.
    [[nodiscard]] IrArithmeticOperator getOperator() const noexcept;

    // Returns the lowered operand instruction.
    [[nodiscard]] const IrExpression& getOperand() const noexcept;

private:
    IrArithmeticOperator operation_;
    std::unique_ptr<IrExpression> operand_;
};

// Represents a lowered binary arithmetic instruction.
class IrBinaryExpression final : public IrExpression {
public:
    // Creates a binary instruction with lowered operands.
    IrBinaryExpression(
        std::unique_ptr<IrExpression> left,
        IrArithmeticOperator operation,
        std::unique_ptr<IrExpression> right,
        types::SemanticType type,
        source::SourceLocation location
    );

    // Returns the lowered left operand.
    [[nodiscard]] const IrExpression& getLeft() const noexcept;

    // Returns the binary operation.
    [[nodiscard]] IrArithmeticOperator getOperator() const noexcept;

    // Returns the lowered right operand.
    [[nodiscard]] const IrExpression& getRight() const noexcept;

private:
    std::unique_ptr<IrExpression> left_;
    IrArithmeticOperator operation_;
    std::unique_ptr<IrExpression> right_;
};

}
