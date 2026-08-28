#pragma once

#include <memory>
#include <string>

#include "crossa/compiler/semantic/TypedExpression.h"
#include "crossa/compiler/source/SourceLocation.h"
#include "crossa/compiler/types/SemanticType.h"

namespace crossa::compiler::semantic {

// Identifies each validated statement shape in a function body.
enum class TypedStatementKind {
    Return,
    Expression,
    Variable
};

// Provides the polymorphic base for typed semantic statements.
class TypedStatement {
public:
    // Releases a concrete typed statement through the base type.
    virtual ~TypedStatement() = default;

    // Returns the concrete typed statement category.
    [[nodiscard]] TypedStatementKind getKind() const noexcept;

    // Returns where this statement begins in source.
    [[nodiscard]] const source::SourceLocation& getLocation() const noexcept;

protected:
    // Creates a typed statement with its category and source location.
    TypedStatement(
        TypedStatementKind kind,
        source::SourceLocation location
    ) noexcept;

private:
    TypedStatementKind kind_;
    source::SourceLocation location_;
};

// Represents one validated return and its typed value expression.
class TypedReturnStatement final : public TypedStatement {
public:
    // Creates a typed return statement.
    TypedReturnStatement(
        std::unique_ptr<TypedExpression> expression,
        source::SourceLocation location
    );

    // Returns the typed logical result expression.
    [[nodiscard]] const TypedExpression& getExpression() const noexcept;

private:
    std::unique_ptr<TypedExpression> expression_;
};

// Represents one validated standalone expression statement.
class TypedExpressionStatement final : public TypedStatement {
public:
    // Creates a typed expression statement.
    TypedExpressionStatement(
        std::unique_ptr<TypedExpression> expression,
        source::SourceLocation location
    );

    // Returns the validated expression.
    [[nodiscard]] const TypedExpression& getExpression() const noexcept;

private:
    std::unique_ptr<TypedExpression> expression_;
};

// Represents one validated local variable and its typed initializer.
class TypedVariableStatement final : public TypedStatement {
public:
    // Creates a typed local variable statement.
    TypedVariableStatement(
        std::string name,
        types::SemanticType type,
        std::unique_ptr<TypedExpression> initializer,
        source::SourceLocation location
    );

    // Returns the local variable name.
    [[nodiscard]] const std::string& getName() const noexcept;

    // Returns the resolved local variable type.
    [[nodiscard]] const types::SemanticType& getType() const noexcept;

    // Returns the validated initializer expression.
    [[nodiscard]] const TypedExpression& getInitializer() const noexcept;

private:
    std::string name_;
    types::SemanticType type_;
    std::unique_ptr<TypedExpression> initializer_;
};

}
