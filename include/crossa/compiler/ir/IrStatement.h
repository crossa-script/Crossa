#pragma once

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "crossa/compiler/ir/IrExpression.h"

namespace crossa::compiler::ir {

// Identifies the statement instructions currently supported by Crossa IR.
enum class IrStatementKind {
    Return,
    Evaluate,
    Local,
    If
};

// Provides the polymorphic base for typed platform-neutral IR statements.
class IrStatement {
public:
    // Releases a concrete IR statement through the base type.
    virtual ~IrStatement() = default;

    // Returns the concrete IR statement category.
    [[nodiscard]] IrStatementKind getKind() const noexcept;

    // Returns the source location of this instruction.
    [[nodiscard]] const source::SourceLocation& getLocation() const noexcept;

protected:
    // Creates an IR statement with its category and source location.
    IrStatement(
        IrStatementKind kind,
        source::SourceLocation location
    ) noexcept;

private:
    IrStatementKind kind_;
    source::SourceLocation location_;
};

// Represents an IR return instruction.
class IrReturnStatement final : public IrStatement {
public:
    // Creates a return instruction with its lowered value.
    IrReturnStatement(
        std::unique_ptr<IrExpression> expression,
        source::SourceLocation location
    );

    // Returns the lowered return value.
    [[nodiscard]] const IrExpression& getExpression() const noexcept;

private:
    std::unique_ptr<IrExpression> expression_;
};

// Represents an IR expression evaluation instruction.
class IrEvaluateStatement final : public IrStatement {
public:
    // Creates an expression evaluation instruction.
    IrEvaluateStatement(
        std::unique_ptr<IrExpression> expression,
        source::SourceLocation location
    );

    // Returns the lowered expression to evaluate.
    [[nodiscard]] const IrExpression& getExpression() const noexcept;

private:
    std::unique_ptr<IrExpression> expression_;
};

// Represents a local declaration and its lowered initializer.
class IrLocalStatement final : public IrStatement {
public:
    // Creates a local instruction with its resolved type and initializer.
    IrLocalStatement(
        std::string name,
        types::SemanticType type,
        std::unique_ptr<IrExpression> initializer,
        source::SourceLocation location
    );

    // Returns the local symbol name.
    [[nodiscard]] const std::string& getName() const noexcept;

    // Returns the local symbol type.
    [[nodiscard]] const types::SemanticType& getType() const noexcept;

    // Returns the lowered initializer.
    [[nodiscard]] const IrExpression& getInitializer() const noexcept;

private:
    std::string name_;
    types::SemanticType type_;
    std::unique_ptr<IrExpression> initializer_;
};

class IrIfStatement final : public IrStatement {
public:
    IrIfStatement(
        std::unique_ptr<IrExpression> condition,
        std::vector<std::unique_ptr<IrStatement>> thenStatements,
        std::optional<std::vector<std::unique_ptr<IrStatement>>> elseStatements,
        source::SourceLocation location
    );

    [[nodiscard]] const IrExpression& getCondition() const noexcept;

    [[nodiscard]] const std::vector<std::unique_ptr<IrStatement>>&
    getThenStatements() const noexcept;

    [[nodiscard]] const std::vector<std::unique_ptr<IrStatement>>*
    getElseStatements() const noexcept;

private:
    std::unique_ptr<IrExpression> condition_;
    std::vector<std::unique_ptr<IrStatement>> thenStatements_;
    std::optional<std::vector<std::unique_ptr<IrStatement>>> elseStatements_;
};

}
