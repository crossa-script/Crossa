#include "crossa/compiler/ast/Declaration.h"

#include <utility>

using namespace std;

namespace crossa::compiler::ast {

    // Creates a declaration with its concrete category.
    Declaration::Declaration(
        DeclarationKind kind,
        source::SourceLocation location
    ) noexcept
        : kind_(kind), location_(location) {}

    // Returns the concrete declaration category.
    DeclarationKind Declaration::getKind() const noexcept {
        return kind_;
    }

    // Returns the source location where this declaration begins.
    const source::SourceLocation& Declaration::getLocation() const noexcept {
        return location_;
    }

    // Creates a source variable with its type and initializer.
    VariableDeclaration::VariableDeclaration(
        string name,
        TypeReference type,
        unique_ptr<Expression> initializer,
        source::SourceLocation location
    )
        : Declaration(DeclarationKind::Variable, location),
          name_(std::move(name)),
          type_(std::move(type)),
          initializer_(std::move(initializer)) {}

    // Returns the variable name.
    const string& VariableDeclaration::getName() const noexcept {
        return name_;
    }

    // Returns the declared variable type.
    const TypeReference& VariableDeclaration::getType() const noexcept {
        return type_;
    }

    // Returns the variable initializer expression.
    const Expression& VariableDeclaration::getInitializer() const noexcept {
        return *initializer_;
    }

    // Creates a function parameter with its declared type.
    Parameter::Parameter(
        string name,
        TypeReference type,
        source::SourceLocation location
    )
        : name_(std::move(name)),
          type_(std::move(type)),
          location_(location) {}

    // Returns the parameter name.
    const string& Parameter::getName() const noexcept {
        return name_;
    }

    // Returns the declared parameter type.
    const TypeReference& Parameter::getType() const noexcept {
        return type_;
    }

    // Returns the source location where this parameter begins.
    const source::SourceLocation& Parameter::getLocation() const noexcept {
        return location_;
    }

    // Creates a function declaration with its complete parsed syntax.
    FunctionDeclaration::FunctionDeclaration(
        string name,
        ExecutionPolicy executionPolicy,
        vector<Parameter> parameters,
        optional<TypeReference> returnType,
        vector<unique_ptr<Statement>> statements,
        source::SourceLocation location
    )
        : Declaration(DeclarationKind::Function, location),
          name_(std::move(name)),
          executionPolicy_(executionPolicy),
          parameters_(std::move(parameters)),
          returnType_(std::move(returnType)),
          statements_(std::move(statements)) {}

    // Returns the function name.
    const string& FunctionDeclaration::getName() const noexcept {
        return name_;
    }

    // Returns the parsed execution policy.
    ExecutionPolicy FunctionDeclaration::getExecutionPolicy() const noexcept {
        return executionPolicy_;
    }

    // Returns the ordered function parameters.
    const vector<Parameter>&
    FunctionDeclaration::getParameters() const noexcept {
        return parameters_;
    }

    // Returns the optional logical result type.
    const TypeReference* FunctionDeclaration::getReturnType() const noexcept {
        return returnType_.has_value() ? &returnType_.value() : nullptr;
    }

    // Returns the ordered function-body statements.
    const vector<unique_ptr<Statement>>&
    FunctionDeclaration::getStatements() const noexcept {
        return statements_;
    }

    // Creates a model field with its declared type.
    ModelField::ModelField(
        string name,
        TypeReference type,
        source::SourceLocation location
    )
        : name_(std::move(name)),
          type_(std::move(type)),
          location_(location) {}

    // Returns the model field name.
    const string& ModelField::getName() const noexcept {
        return name_;
    }

    // Returns the declared field type.
    const TypeReference& ModelField::getType() const noexcept {
        return type_;
    }

    // Returns the source location where this field begins.
    const source::SourceLocation& ModelField::getLocation() const noexcept {
        return location_;
    }

    // Creates a model declaration with its ordered fields.
    ModelDeclaration::ModelDeclaration(
        string name,
        vector<ModelField> fields,
        source::SourceLocation location
    )
        : Declaration(DeclarationKind::Model, location),
          name_(std::move(name)),
          fields_(std::move(fields)) {}

    // Returns the model name.
    const string& ModelDeclaration::getName() const noexcept {
        return name_;
    }

    // Returns the ordered model fields.
    const vector<ModelField>& ModelDeclaration::getFields() const noexcept {
        return fields_;
    }

    // Creates a configuration entry from its key and value expression.
    ConfigEntry::ConfigEntry(
        string name,
        unique_ptr<Expression> value,
        source::SourceLocation location
    )
        : name_(std::move(name)),
          value_(std::move(value)),
          location_(location) {}

    // Returns the configuration key.
    const string& ConfigEntry::getName() const noexcept {
        return name_;
    }

    // Returns the configuration value expression.
    const Expression& ConfigEntry::getValue() const noexcept {
        return *value_;
    }

    // Returns the source location where this entry begins.
    const source::SourceLocation& ConfigEntry::getLocation() const noexcept {
        return location_;
    }

    // Creates a config declaration with its parsed entries.
    ConfigDeclaration::ConfigDeclaration(
        vector<ConfigEntry> entries,
        source::SourceLocation location
    )
        : Declaration(DeclarationKind::Config, location),
          entries_(std::move(entries)) {}

    // Returns the ordered configuration entries.
    const vector<ConfigEntry>& ConfigDeclaration::getEntries() const noexcept {
        return entries_;
    }

    // Creates a top-level expression declaration.
    ExpressionDeclaration::ExpressionDeclaration(
        unique_ptr<Expression> expression,
        source::SourceLocation location
    )
        : Declaration(DeclarationKind::Expression, location),
          expression_(std::move(expression)) {}

    // Returns the top-level expression to execute.
    const Expression& ExpressionDeclaration::getExpression() const noexcept {
        return *expression_;
    }

}
