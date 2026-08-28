#include "crossa/compiler/semantic/TypedStatement.h"

#include <utility>

using namespace std;

namespace crossa::compiler::semantic {

    // Creates a typed statement with its category and source location.
    TypedStatement::TypedStatement(
        TypedStatementKind kind,
        source::SourceLocation location
    ) noexcept
        : kind_(kind), location_(location) {}

    // Returns the concrete typed statement category.
    TypedStatementKind TypedStatement::getKind() const noexcept {
        return kind_;
    }

    // Returns where this statement begins in source.
    const source::SourceLocation&
    TypedStatement::getLocation() const noexcept {
        return location_;
    }

    // Creates a typed return statement.
    TypedReturnStatement::TypedReturnStatement(
        unique_ptr<TypedExpression> expression,
        source::SourceLocation location
    )
        : TypedStatement(TypedStatementKind::Return, location),
          expression_(std::move(expression)) {}

    // Returns the typed logical result expression.
    const TypedExpression&
    TypedReturnStatement::getExpression() const noexcept {
        return *expression_;
    }

    // Creates a typed expression statement.
    TypedExpressionStatement::TypedExpressionStatement(
        unique_ptr<TypedExpression> expression,
        source::SourceLocation location
    )
        : TypedStatement(TypedStatementKind::Expression, location),
          expression_(std::move(expression)) {}

    // Returns the validated expression.
    const TypedExpression&
    TypedExpressionStatement::getExpression() const noexcept {
        return *expression_;
    }

    // Creates a typed local variable statement.
    TypedVariableStatement::TypedVariableStatement(
        string name,
        types::SemanticType type,
        unique_ptr<TypedExpression> initializer,
        source::SourceLocation location
    )
        : TypedStatement(TypedStatementKind::Variable, location),
          name_(std::move(name)),
          type_(std::move(type)),
          initializer_(std::move(initializer)) {}

    // Returns the local variable name.
    const string& TypedVariableStatement::getName() const noexcept {
        return name_;
    }

    // Returns the resolved local variable type.
    const types::SemanticType&
    TypedVariableStatement::getType() const noexcept {
        return type_;
    }

    // Returns the validated initializer expression.
    const TypedExpression&
    TypedVariableStatement::getInitializer() const noexcept {
        return *initializer_;
    }

}
