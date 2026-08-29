#pragma once

#include <memory>
#include <string>
#include <vector>

#include "crossa/compiler/semantic/TypedExpression.h"
#include "crossa/compiler/semantic/TypedStatement.h"
#include "crossa/compiler/source/SourceLocation.h"
#include "crossa/compiler/types/SemanticType.h"

namespace crossa::compiler::semantic {

// Identifies each declaration category in the typed semantic model.
enum class TypedDeclarationKind {
    Variable,
    Model,
    Function,
    Config,
    Expression
};

// Defines the validated execution policy attached to a function.
enum class SemanticExecutionPolicy {
    Sync,
    Async,
    AsyncAfter
};

// Provides the polymorphic base for typed semantic declarations.
class TypedDeclaration {
public:
    // Releases a concrete typed declaration through the base type.
    virtual ~TypedDeclaration() = default;

    // Returns the concrete typed declaration category.
    [[nodiscard]] TypedDeclarationKind getKind() const noexcept;

    // Returns where this declaration begins in source.
    [[nodiscard]] const source::SourceLocation& getLocation() const noexcept;

protected:
    // Creates a typed declaration with its category and location.
    TypedDeclaration(
        TypedDeclarationKind kind,
        source::SourceLocation location
    ) noexcept;

private:
    TypedDeclarationKind kind_;
    source::SourceLocation location_;
};

// Represents one validated source variable and typed initializer.
class TypedVariableDeclaration final : public TypedDeclaration {
public:
    // Creates a typed source variable declaration.
    TypedVariableDeclaration(
        std::string name,
        types::SemanticType type,
        std::unique_ptr<TypedExpression> initializer,
        source::SourceLocation location
    );

    // Returns the source variable name.
    [[nodiscard]] const std::string& getName() const noexcept;

    // Returns the resolved source variable type.
    [[nodiscard]] const types::SemanticType& getType() const noexcept;

    // Returns the validated initializer expression.
    [[nodiscard]] const TypedExpression& getInitializer() const noexcept;

private:
    std::string name_;
    types::SemanticType type_;
    std::unique_ptr<TypedExpression> initializer_;
};

// Stores one resolved function parameter in declaration order.
class TypedParameter final {
public:
    // Creates a typed function parameter.
    TypedParameter(
        std::string name,
        types::SemanticType type,
        source::SourceLocation location
    );

    // Returns the parameter name.
    [[nodiscard]] const std::string& getName() const noexcept;

    // Returns the resolved parameter type.
    [[nodiscard]] const types::SemanticType& getType() const noexcept;

    // Returns where this parameter begins in source.
    [[nodiscard]] const source::SourceLocation& getLocation() const noexcept;

private:
    std::string name_;
    types::SemanticType type_;
    source::SourceLocation location_;
};

// Represents one validated function signature, policy, and typed body.
class TypedFunctionDeclaration final : public TypedDeclaration {
public:
    // Creates a complete typed function declaration.
    TypedFunctionDeclaration(
        std::string name,
        SemanticExecutionPolicy executionPolicy,
        std::vector<TypedParameter> parameters,
        types::SemanticType returnType,
        std::vector<std::unique_ptr<TypedStatement>> statements,
        source::SourceLocation location
    );

    // Returns the function name.
    [[nodiscard]] const std::string& getName() const noexcept;

    // Returns the validated execution policy.
    [[nodiscard]] SemanticExecutionPolicy getExecutionPolicy() const noexcept;

    // Returns the ordered typed parameters.
    [[nodiscard]] const std::vector<TypedParameter>&
    getParameters() const noexcept;

    // Returns the logical result type or internal Unit type.
    [[nodiscard]] const types::SemanticType& getReturnType() const noexcept;

    // Returns the ordered typed body statements.
    [[nodiscard]] const std::vector<std::unique_ptr<TypedStatement>>&
    getStatements() const noexcept;

private:
    std::string name_;
    SemanticExecutionPolicy executionPolicy_;
    std::vector<TypedParameter> parameters_;
    types::SemanticType returnType_;
    std::vector<std::unique_ptr<TypedStatement>> statements_;
};

// Stores one validated model field in stable declaration order.
class TypedModelField final {
public:
    // Creates a typed model field.
    TypedModelField(
        std::string name,
        types::SemanticType type,
        source::SourceLocation location
    );

    // Returns the model field name.
    [[nodiscard]] const std::string& getName() const noexcept;

    // Returns the resolved model field type.
    [[nodiscard]] const types::SemanticType& getType() const noexcept;

    // Returns where this model field begins in source.
    [[nodiscard]] const source::SourceLocation& getLocation() const noexcept;

private:
    std::string name_;
    types::SemanticType type_;
    source::SourceLocation location_;
};

// Represents one validated named model and its ordered schema.
class TypedModelDeclaration final : public TypedDeclaration {
public:
    // Creates a typed model declaration.
    TypedModelDeclaration(
        std::string name,
        std::vector<TypedModelField> fields,
        source::SourceLocation location
    );

    // Returns the model name.
    [[nodiscard]] const std::string& getName() const noexcept;

    // Returns the ordered typed model fields.
    [[nodiscard]] const std::vector<TypedModelField>& getFields() const noexcept;

private:
    std::string name_;
    std::vector<TypedModelField> fields_;
};

// Stores one validated configuration key and typed value.
class TypedConfigEntry final {
public:
    // Creates a typed configuration entry.
    TypedConfigEntry(
        std::string name,
        types::SemanticType type,
        std::unique_ptr<TypedExpression> value,
        source::SourceLocation location
    );

    // Returns the validated configuration key.
    [[nodiscard]] const std::string& getName() const noexcept;

    // Returns the required configuration value type.
    [[nodiscard]] const types::SemanticType& getType() const noexcept;

    // Returns the validated configuration value.
    [[nodiscard]] const TypedExpression& getValue() const noexcept;

    // Returns where this configuration entry begins in source.
    [[nodiscard]] const source::SourceLocation& getLocation() const noexcept;

private:
    std::string name_;
    types::SemanticType type_;
    std::unique_ptr<TypedExpression> value_;
    source::SourceLocation location_;
};

// Represents one validated declarative config block.
class TypedConfigDeclaration final : public TypedDeclaration {
public:
    // Creates a typed config declaration.
    TypedConfigDeclaration(
        std::vector<TypedConfigEntry> entries,
        source::SourceLocation location
    );

    // Returns the ordered validated configuration entries.
    [[nodiscard]] const std::vector<TypedConfigEntry>&
    getEntries() const noexcept;

private:
    std::vector<TypedConfigEntry> entries_;
};

// Represents one validated top-level executable expression.
// Its result is evaluated only when the native program executes this unit.
class TypedExpressionDeclaration final : public TypedDeclaration {
public:
    // Creates a typed top-level expression declaration.
    TypedExpressionDeclaration(
        std::unique_ptr<TypedExpression> expression,
        source::SourceLocation location
    );

    // Returns the validated top-level expression.
    [[nodiscard]] const TypedExpression& getExpression() const noexcept;

private:
    std::unique_ptr<TypedExpression> expression_;
};

}
