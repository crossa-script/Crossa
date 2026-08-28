#pragma once

#include <memory>
#include <string>

#include "crossa/compiler/ast/Expression.h"
#include "crossa/compiler/ast/TypeReference.h"

namespace crossa::compiler::ast {

// Identifies the concrete statement shape stored in a function body.
enum class StatementKind {
    Return,
    Expression,
    Variable
};

// Provides the polymorphic base for Crossa function-body statements.
// getKind() enables deterministic parser and semantic traversal.
class Statement {
public:
    // Releases a concrete statement through the base type.
    virtual ~Statement() = default;

    // Returns the concrete statement category.
    [[nodiscard]] StatementKind getKind() const noexcept;

protected:
    // Creates a statement with its concrete category.
    explicit Statement(StatementKind kind) noexcept;

private:
    StatementKind kind_;
};

// Represents a re statement and its required value expression.
// getExpression() exposes the logical function result expression.
class ReturnStatement final : public Statement {
public:
    // Creates a return statement that owns one expression.
    explicit ReturnStatement(std::unique_ptr<Expression> expression);

    // Returns the owned return expression.
    [[nodiscard]] const Expression& getExpression() const noexcept;

private:
    std::unique_ptr<Expression> expression_;
};

// Represents a standalone expression in a function body.
// getExpression() exposes calls such as the print builtin.
class ExpressionStatement final : public Statement {
public:
    // Creates an expression statement that owns one expression.
    explicit ExpressionStatement(std::unique_ptr<Expression> expression);

    // Returns the owned statement expression.
    [[nodiscard]] const Expression& getExpression() const noexcept;

private:
    std::unique_ptr<Expression> expression_;
};

// Represents a typed local variable and its initializer expression.
// Its accessors expose the declaration for semantic validation.
class VariableStatement final : public Statement {
public:
    // Creates a local variable statement with its type and initializer.
    VariableStatement(
        std::string name,
        TypeReference type,
        std::unique_ptr<Expression> initializer
    );

    // Returns the local variable name.
    [[nodiscard]] const std::string& getName() const noexcept;

    // Returns the declared variable type.
    [[nodiscard]] const TypeReference& getType() const noexcept;

    // Returns the variable initializer expression.
    [[nodiscard]] const Expression& getInitializer() const noexcept;

private:
    std::string name_;
    TypeReference type_;
    std::unique_ptr<Expression> initializer_;
};

}
