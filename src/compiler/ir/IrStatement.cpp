#include "crossa/compiler/ir/IrStatement.h"

#include <utility>

using namespace std;

namespace crossa::compiler::ir {

    // Creates an IR statement with its category and source location.
    IrStatement::IrStatement(
        IrStatementKind kind,
        source::SourceLocation location
    ) noexcept
        : kind_(kind), location_(location) {}

    // Returns the concrete IR statement category.
    IrStatementKind IrStatement::getKind() const noexcept {
        return kind_;
    }

    // Returns the source location of this instruction.
    const source::SourceLocation& IrStatement::getLocation() const noexcept {
        return location_;
    }

    // Creates a return instruction with its lowered value.
    IrReturnStatement::IrReturnStatement(
        unique_ptr<IrExpression> expression,
        source::SourceLocation location
    )
        : IrStatement(IrStatementKind::Return, location),
          expression_(std::move(expression)) {}

    // Returns the lowered return value.
    const IrExpression& IrReturnStatement::getExpression() const noexcept {
        return *expression_;
    }

    // Creates an expression evaluation instruction.
    IrEvaluateStatement::IrEvaluateStatement(
        unique_ptr<IrExpression> expression,
        source::SourceLocation location
    )
        : IrStatement(IrStatementKind::Evaluate, location),
          expression_(std::move(expression)) {}

    // Returns the lowered expression to evaluate.
    const IrExpression& IrEvaluateStatement::getExpression() const noexcept {
        return *expression_;
    }

    // Creates a local instruction with its resolved type and initializer.
    IrLocalStatement::IrLocalStatement(
        string name,
        types::SemanticType type,
        unique_ptr<IrExpression> initializer,
        source::SourceLocation location
    )
        : IrStatement(IrStatementKind::Local, location),
          name_(std::move(name)),
          type_(std::move(type)),
          initializer_(std::move(initializer)) {}

    // Returns the local symbol name.
    const string& IrLocalStatement::getName() const noexcept {
        return name_;
    }

    // Returns the local symbol type.
    const types::SemanticType& IrLocalStatement::getType() const noexcept {
        return type_;
    }

    // Returns the lowered initializer.
    const IrExpression& IrLocalStatement::getInitializer() const noexcept {
        return *initializer_;
    }

}
