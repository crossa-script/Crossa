#include "crossa/compiler/semantic/TypedDeclaration.h"

#include <utility>

using namespace std;

namespace crossa::compiler::semantic {

    // Creates a typed declaration with its category and location.
    TypedDeclaration::TypedDeclaration(
        TypedDeclarationKind kind,
        source::SourceLocation location
    ) noexcept
        : kind_(kind), location_(location) {}

    // Returns the concrete typed declaration category.
    TypedDeclarationKind TypedDeclaration::getKind() const noexcept {
        return kind_;
    }

    // Returns where this declaration begins in source.
    const source::SourceLocation&
    TypedDeclaration::getLocation() const noexcept {
        return location_;
    }

    // Creates a typed source variable declaration.
    TypedVariableDeclaration::TypedVariableDeclaration(
        string name,
        types::SemanticType type,
        unique_ptr<TypedExpression> initializer,
        source::SourceLocation location
    )
        : TypedDeclaration(TypedDeclarationKind::Variable, location),
          name_(std::move(name)),
          type_(std::move(type)),
          initializer_(std::move(initializer)) {}

    // Returns the source variable name.
    const string& TypedVariableDeclaration::getName() const noexcept {
        return name_;
    }

    // Returns the resolved source variable type.
    const types::SemanticType&
    TypedVariableDeclaration::getType() const noexcept {
        return type_;
    }

    // Returns the validated initializer expression.
    const TypedExpression&
    TypedVariableDeclaration::getInitializer() const noexcept {
        return *initializer_;
    }

    // Creates a typed function parameter.
    TypedParameter::TypedParameter(
        string name,
        types::SemanticType type,
        source::SourceLocation location
    )
        : name_(std::move(name)),
          type_(std::move(type)),
          location_(location) {}

    // Returns the parameter name.
    const string& TypedParameter::getName() const noexcept {
        return name_;
    }

    // Returns the resolved parameter type.
    const types::SemanticType& TypedParameter::getType() const noexcept {
        return type_;
    }

    // Returns where this parameter begins in source.
    const source::SourceLocation& TypedParameter::getLocation() const noexcept {
        return location_;
    }

    // Creates a complete typed function declaration.
    TypedFunctionDeclaration::TypedFunctionDeclaration(
        string name,
        SemanticExecutionPolicy executionPolicy,
        vector<TypedParameter> parameters,
        types::SemanticType returnType,
        vector<unique_ptr<TypedStatement>> statements,
        source::SourceLocation location
    )
        : TypedDeclaration(TypedDeclarationKind::Function, location),
          name_(std::move(name)),
          executionPolicy_(executionPolicy),
          parameters_(std::move(parameters)),
          returnType_(std::move(returnType)),
          statements_(std::move(statements)) {}

    // Returns the function name.
    const string& TypedFunctionDeclaration::getName() const noexcept {
        return name_;
    }

    // Returns the validated execution policy.
    SemanticExecutionPolicy
    TypedFunctionDeclaration::getExecutionPolicy() const noexcept {
        return executionPolicy_;
    }

    // Returns the ordered typed parameters.
    const vector<TypedParameter>&
    TypedFunctionDeclaration::getParameters() const noexcept {
        return parameters_;
    }

    // Returns the logical result type or internal Unit type.
    const types::SemanticType&
    TypedFunctionDeclaration::getReturnType() const noexcept {
        return returnType_;
    }

    // Returns the ordered typed body statements.
    const vector<unique_ptr<TypedStatement>>&
    TypedFunctionDeclaration::getStatements() const noexcept {
        return statements_;
    }

    // Creates a typed model field.
    TypedModelField::TypedModelField(
        string name,
        types::SemanticType type,
        source::SourceLocation location
    )
        : name_(std::move(name)),
          type_(std::move(type)),
          location_(location) {}

    // Returns the model field name.
    const string& TypedModelField::getName() const noexcept {
        return name_;
    }

    // Returns the resolved model field type.
    const types::SemanticType& TypedModelField::getType() const noexcept {
        return type_;
    }

    // Returns where this model field begins in source.
    const source::SourceLocation& TypedModelField::getLocation() const noexcept {
        return location_;
    }

    // Creates a typed model declaration.
    TypedModelDeclaration::TypedModelDeclaration(
        string name,
        vector<TypedModelField> fields,
        source::SourceLocation location
    )
        : TypedDeclaration(TypedDeclarationKind::Model, location),
          name_(std::move(name)),
          fields_(std::move(fields)) {}

    // Returns the model name.
    const string& TypedModelDeclaration::getName() const noexcept {
        return name_;
    }

    // Returns the ordered typed model fields.
    const vector<TypedModelField>&
    TypedModelDeclaration::getFields() const noexcept {
        return fields_;
    }

    // Creates a typed configuration entry.
    TypedConfigEntry::TypedConfigEntry(
        string name,
        types::SemanticType type,
        unique_ptr<TypedExpression> value,
        source::SourceLocation location
    )
        : name_(std::move(name)),
          type_(std::move(type)),
          value_(std::move(value)),
          location_(location) {}

    // Returns the validated configuration key.
    const string& TypedConfigEntry::getName() const noexcept {
        return name_;
    }

    // Returns the required configuration value type.
    const types::SemanticType& TypedConfigEntry::getType() const noexcept {
        return type_;
    }

    // Returns the validated configuration value.
    const TypedExpression& TypedConfigEntry::getValue() const noexcept {
        return *value_;
    }

    // Returns where this configuration entry begins in source.
    const source::SourceLocation& TypedConfigEntry::getLocation() const noexcept {
        return location_;
    }

    // Creates a typed config declaration.
    TypedConfigDeclaration::TypedConfigDeclaration(
        vector<TypedConfigEntry> entries,
        source::SourceLocation location
    )
        : TypedDeclaration(TypedDeclarationKind::Config, location),
          entries_(std::move(entries)) {}

    // Returns the ordered validated configuration entries.
    const vector<TypedConfigEntry>&
    TypedConfigDeclaration::getEntries() const noexcept {
        return entries_;
    }

}
