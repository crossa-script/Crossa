#pragma once

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "crossa/compiler/ast/Expression.h"
#include "crossa/compiler/ast/Statement.h"
#include "crossa/compiler/ast/TypeReference.h"

namespace crossa::compiler::ast {

// Identifies each top-level declaration supported by the initial parser.
enum class DeclarationKind {
    Variable,
    Model,
    Function,
    Config
};

// Identifies a function's optional execution-policy annotation.
enum class ExecutionPolicy {
    None,
    Sync,
    Async,
    AsyncAfter
};

// Provides the polymorphic base for top-level Crossa declarations.
// getKind() enables deterministic traversal by later compiler stages.
class Declaration {
public:
    // Releases a concrete declaration through the base type.
    virtual ~Declaration() = default;

    // Returns the concrete declaration category.
    [[nodiscard]] DeclarationKind getKind() const noexcept;

    // Returns the source location where this declaration begins.
    [[nodiscard]] const source::SourceLocation& getLocation() const noexcept;

protected:
    // Creates a declaration with its concrete category.
    Declaration(
        DeclarationKind kind,
        source::SourceLocation location
    ) noexcept;

private:
    DeclarationKind kind_;
    source::SourceLocation location_;
};

// Represents one typed top-level variable and its initializer.
// Its accessors expose the declaration for semantic validation.
class VariableDeclaration final : public Declaration {
public:
    // Creates a source variable with its type and initializer.
    VariableDeclaration(
        std::string name,
        TypeReference type,
        std::unique_ptr<Expression> initializer,
        source::SourceLocation location
    );

    // Returns the variable name.
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

// Stores one typed function parameter in declaration order.
// getName() and getType() expose its unresolved source contract.
class Parameter final {
public:
    // Creates a function parameter with its declared type.
    Parameter(
        std::string name,
        TypeReference type,
        source::SourceLocation location
    );

    // Returns the parameter name.
    [[nodiscard]] const std::string& getName() const noexcept;

    // Returns the declared parameter type.
    [[nodiscard]] const TypeReference& getType() const noexcept;

    // Returns the source location where this parameter begins.
    [[nodiscard]] const source::SourceLocation& getLocation() const noexcept;

private:
    std::string name_;
    TypeReference type_;
    source::SourceLocation location_;
};

// Represents one function, its annotation, signature, and body statements.
// Its accessors expose all syntax required by semantic analysis.
class FunctionDeclaration final : public Declaration {
public:
    // Creates a function declaration with its complete parsed syntax.
    FunctionDeclaration(
        std::string name,
        ExecutionPolicy executionPolicy,
        std::vector<Parameter> parameters,
        std::optional<TypeReference> returnType,
        std::vector<std::unique_ptr<Statement>> statements,
        source::SourceLocation location
    );

    // Returns the function name.
    [[nodiscard]] const std::string& getName() const noexcept;

    // Returns the parsed execution policy.
    [[nodiscard]] ExecutionPolicy getExecutionPolicy() const noexcept;

    // Returns the ordered function parameters.
    [[nodiscard]] const std::vector<Parameter>& getParameters() const noexcept;

    // Returns the optional logical result type.
    [[nodiscard]] const TypeReference* getReturnType() const noexcept;

    // Returns the ordered function-body statements.
    [[nodiscard]] const std::vector<std::unique_ptr<Statement>>&
    getStatements() const noexcept;

private:
    std::string name_;
    ExecutionPolicy executionPolicy_;
    std::vector<Parameter> parameters_;
    std::optional<TypeReference> returnType_;
    std::vector<std::unique_ptr<Statement>> statements_;
};

// Stores one typed model field in stable source order.
// getName() and getType() expose the model schema entry.
class ModelField final {
public:
    // Creates a model field with its declared type.
    ModelField(
        std::string name,
        TypeReference type,
        source::SourceLocation location
    );

    // Returns the model field name.
    [[nodiscard]] const std::string& getName() const noexcept;

    // Returns the declared field type.
    [[nodiscard]] const TypeReference& getType() const noexcept;

    // Returns the source location where this field begins.
    [[nodiscard]] const source::SourceLocation& getLocation() const noexcept;

private:
    std::string name_;
    TypeReference type_;
    source::SourceLocation location_;
};

// Represents a named model and its ordered typed fields.
// getName() and getFields() expose the complete parsed schema.
class ModelDeclaration final : public Declaration {
public:
    // Creates a model declaration with its ordered fields.
    ModelDeclaration(
        std::string name,
        std::vector<ModelField> fields,
        source::SourceLocation location
    );

    // Returns the model name.
    [[nodiscard]] const std::string& getName() const noexcept;

    // Returns the ordered model fields.
    [[nodiscard]] const std::vector<ModelField>& getFields() const noexcept;

private:
    std::string name_;
    std::vector<ModelField> fields_;
};

// Stores one config key and its value expression.
// getName() and getValue() expose the unvalidated configuration entry.
class ConfigEntry final {
public:
    // Creates a configuration entry from its key and value expression.
    ConfigEntry(
        std::string name,
        std::unique_ptr<Expression> value,
        source::SourceLocation location
    );

    // Returns the configuration key.
    [[nodiscard]] const std::string& getName() const noexcept;

    // Returns the configuration value expression.
    [[nodiscard]] const Expression& getValue() const noexcept;

    // Returns the source location where this entry begins.
    [[nodiscard]] const source::SourceLocation& getLocation() const noexcept;

private:
    std::string name_;
    std::unique_ptr<Expression> value_;
    source::SourceLocation location_;
};

// Represents a config block and its ordered entries.
// getEntries() exposes values for later config validation.
class ConfigDeclaration final : public Declaration {
public:
    // Creates a config declaration with its parsed entries.
    ConfigDeclaration(
        std::vector<ConfigEntry> entries,
        source::SourceLocation location
    );

    // Returns the ordered configuration entries.
    [[nodiscard]] const std::vector<ConfigEntry>& getEntries() const noexcept;

private:
    std::vector<ConfigEntry> entries_;
};

}
