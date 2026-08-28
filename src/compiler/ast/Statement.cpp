#include "crossa/compiler/ast/Statement.h"

#include <utility>

using namespace std;

namespace crossa::compiler::ast {

    // Creates a statement with its concrete category.
    Statement::Statement(
        StatementKind kind,
        source::SourceLocation location
    ) noexcept
        : kind_(kind), location_(location) {}

    // Returns the concrete statement category.
    StatementKind Statement::getKind() const noexcept {
        return kind_;
    }

    // Returns the source location where this statement begins.
    const source::SourceLocation& Statement::getLocation() const noexcept {
        return location_;
    }

    // Creates a return statement that owns one expression.
    ReturnStatement::ReturnStatement(
        unique_ptr<Expression> expression,
        source::SourceLocation location
    )
        : Statement(StatementKind::Return, location),
          expression_(std::move(expression)) {}

    // Returns the owned return expression.
    const Expression& ReturnStatement::getExpression() const noexcept {
        return *expression_;
    }

    // Creates an expression statement that owns one expression.
    ExpressionStatement::ExpressionStatement(
        unique_ptr<Expression> expression,
        source::SourceLocation location
    )
        : Statement(StatementKind::Expression, location),
          expression_(std::move(expression)) {}

    // Returns the owned statement expression.
    const Expression& ExpressionStatement::getExpression() const noexcept {
        return *expression_;
    }

    // Creates a local variable statement with its type and initializer.
    VariableStatement::VariableStatement(
        string name,
        TypeReference type,
        unique_ptr<Expression> initializer,
        source::SourceLocation location
    )
        : Statement(StatementKind::Variable, location),
          name_(std::move(name)),
          type_(std::move(type)),
          initializer_(std::move(initializer)) {}

    // Returns the local variable name.
    const string& VariableStatement::getName() const noexcept {
        return name_;
    }

    // Returns the declared variable type.
    const TypeReference& VariableStatement::getType() const noexcept {
        return type_;
    }

    // Returns the variable initializer expression.
    const Expression& VariableStatement::getInitializer() const noexcept {
        return *initializer_;
    }

}
