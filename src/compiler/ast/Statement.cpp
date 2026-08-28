#include "crossa/compiler/ast/Statement.h"

#include <utility>

using namespace std;

namespace crossa::compiler::ast {

    // Creates a statement with its concrete category.
    Statement::Statement(StatementKind kind) noexcept : kind_(kind) {}

    // Returns the concrete statement category.
    StatementKind Statement::getKind() const noexcept {
        return kind_;
    }

    // Creates a return statement that owns one expression.
    ReturnStatement::ReturnStatement(unique_ptr<Expression> expression)
        : Statement(StatementKind::Return),
          expression_(std::move(expression)) {}

    // Returns the owned return expression.
    const Expression& ReturnStatement::getExpression() const noexcept {
        return *expression_;
    }

    // Creates an expression statement that owns one expression.
    ExpressionStatement::ExpressionStatement(unique_ptr<Expression> expression)
        : Statement(StatementKind::Expression),
          expression_(std::move(expression)) {}

    // Returns the owned statement expression.
    const Expression& ExpressionStatement::getExpression() const noexcept {
        return *expression_;
    }

    // Creates a local variable statement with its type and initializer.
    VariableStatement::VariableStatement(
        string name,
        TypeReference type,
        unique_ptr<Expression> initializer
    )
        : Statement(StatementKind::Variable),
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
