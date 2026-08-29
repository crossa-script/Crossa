#include "crossa/compiler/ir/IrDeclaration.h"

#include <utility>

using namespace std;

namespace crossa::compiler::ir {

    // Creates an IR declaration with its category and location.
    IrDeclaration::IrDeclaration(
        IrDeclarationKind kind,
        source::SourceLocation location
    ) noexcept
        : kind_(kind), location_(location) {}

    // Returns the concrete IR declaration category.
    IrDeclarationKind IrDeclaration::getKind() const noexcept {
        return kind_;
    }

    // Returns the source location of this declaration.
    const source::SourceLocation& IrDeclaration::getLocation() const noexcept {
        return location_;
    }

    // Creates a lowered source variable declaration.
    IrVariableDeclaration::IrVariableDeclaration(
        string name,
        types::SemanticType type,
        unique_ptr<IrExpression> initializer,
        source::SourceLocation location
    )
        : IrDeclaration(IrDeclarationKind::Variable, location),
          name_(std::move(name)),
          type_(std::move(type)),
          initializer_(std::move(initializer)) {}

    // Returns the variable name.
    const string& IrVariableDeclaration::getName() const noexcept {
        return name_;
    }

    // Returns the variable type.
    const types::SemanticType& IrVariableDeclaration::getType() const noexcept {
        return type_;
    }

    // Returns the lowered initializer.
    const IrExpression& IrVariableDeclaration::getInitializer() const noexcept {
        return *initializer_;
    }

    // Creates a lowered function parameter.
    IrParameter::IrParameter(
        string name,
        types::SemanticType type,
        source::SourceLocation location
    )
        : name_(std::move(name)),
          type_(std::move(type)),
          location_(location) {}

    // Returns the parameter name.
    const string& IrParameter::getName() const noexcept {
        return name_;
    }

    // Returns the parameter type.
    const types::SemanticType& IrParameter::getType() const noexcept {
        return type_;
    }

    // Returns the parameter source location.
    const source::SourceLocation& IrParameter::getLocation() const noexcept {
        return location_;
    }

    // Creates a lowered function declaration.
    IrFunctionDeclaration::IrFunctionDeclaration(
        string name,
        IrExecutionPolicy executionPolicy,
        vector<IrParameter> parameters,
        types::SemanticType returnType,
        vector<unique_ptr<IrStatement>> statements,
        source::SourceLocation location
    )
        : IrDeclaration(IrDeclarationKind::Function, location),
          name_(std::move(name)),
          executionPolicy_(executionPolicy),
          parameters_(std::move(parameters)),
          returnType_(std::move(returnType)),
          statements_(std::move(statements)) {}

    // Returns the function name.
    const string& IrFunctionDeclaration::getName() const noexcept {
        return name_;
    }

    // Returns the function execution policy.
    IrExecutionPolicy IrFunctionDeclaration::getExecutionPolicy() const noexcept {
        return executionPolicy_;
    }

    // Returns the ordered function parameters.
    const vector<IrParameter>&
    IrFunctionDeclaration::getParameters() const noexcept {
        return parameters_;
    }

    // Returns the logical function result type.
    const types::SemanticType& IrFunctionDeclaration::getReturnType() const noexcept {
        return returnType_;
    }

    // Returns the ordered lowered function instructions.
    const vector<unique_ptr<IrStatement>>&
    IrFunctionDeclaration::getStatements() const noexcept {
        return statements_;
    }

    // Creates a lowered model field.
    IrModelField::IrModelField(
        string name,
        types::SemanticType type,
        source::SourceLocation location
    )
        : name_(std::move(name)),
          type_(std::move(type)),
          location_(location) {}

    // Returns the field name.
    const string& IrModelField::getName() const noexcept {
        return name_;
    }

    // Returns the field type.
    const types::SemanticType& IrModelField::getType() const noexcept {
        return type_;
    }

    // Returns the field source location.
    const source::SourceLocation& IrModelField::getLocation() const noexcept {
        return location_;
    }

    // Creates a lowered model declaration.
    IrModelDeclaration::IrModelDeclaration(
        string name,
        vector<IrModelField> fields,
        source::SourceLocation location
    )
        : IrDeclaration(IrDeclarationKind::Model, location),
          name_(std::move(name)),
          fields_(std::move(fields)) {}

    // Returns the model name.
    const string& IrModelDeclaration::getName() const noexcept {
        return name_;
    }

    // Returns the ordered lowered model fields.
    const vector<IrModelField>& IrModelDeclaration::getFields() const noexcept {
        return fields_;
    }

    // Creates a lowered config entry.
    IrConfigEntry::IrConfigEntry(
        string name,
        types::SemanticType type,
        unique_ptr<IrExpression> value,
        source::SourceLocation location
    )
        : name_(std::move(name)),
          type_(std::move(type)),
          value_(std::move(value)),
          location_(location) {}

    // Returns the config key.
    const string& IrConfigEntry::getName() const noexcept {
        return name_;
    }

    // Returns the config value type.
    const types::SemanticType& IrConfigEntry::getType() const noexcept {
        return type_;
    }

    // Returns the lowered config value.
    const IrExpression& IrConfigEntry::getValue() const noexcept {
        return *value_;
    }

    // Returns the config entry source location.
    const source::SourceLocation& IrConfigEntry::getLocation() const noexcept {
        return location_;
    }

    // Creates a lowered config declaration.
    IrConfigDeclaration::IrConfigDeclaration(
        vector<IrConfigEntry> entries,
        source::SourceLocation location
    )
        : IrDeclaration(IrDeclarationKind::Config, location),
          entries_(std::move(entries)) {}

    // Returns the ordered lowered config entries.
    const vector<IrConfigEntry>& IrConfigDeclaration::getEntries() const noexcept {
        return entries_;
    }

    // Creates a top-level expression instruction.
    IrExpressionDeclaration::IrExpressionDeclaration(
        unique_ptr<IrExpression> expression,
        source::SourceLocation location
    )
        : IrDeclaration(IrDeclarationKind::Expression, location),
          expression_(std::move(expression)) {}

    // Returns the lowered top-level expression.
    const IrExpression& IrExpressionDeclaration::getExpression() const noexcept {
        return *expression_;
    }

}
