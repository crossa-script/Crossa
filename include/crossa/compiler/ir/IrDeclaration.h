#pragma once

#include <memory>
#include <string>
#include <vector>

#include "crossa/compiler/ir/IrStatement.h"

namespace crossa::compiler::ir {

// Identifies the top-level declarations represented by Crossa IR.
enum class IrDeclarationKind {
    Variable,
    Model,
    Function,
    Config
};

// Defines scheduling policy on a lowered function.
enum class IrExecutionPolicy {
    Sync,
    Async,
    AsyncAfter
};

// Provides the polymorphic base for top-level platform-neutral IR nodes.
class IrDeclaration {
public:
    // Releases a concrete IR declaration through the base type.
    virtual ~IrDeclaration() = default;

    // Returns the concrete IR declaration category.
    [[nodiscard]] IrDeclarationKind getKind() const noexcept;

    // Returns the source location of this declaration.
    [[nodiscard]] const source::SourceLocation& getLocation() const noexcept;

protected:
    // Creates an IR declaration with its category and location.
    IrDeclaration(
        IrDeclarationKind kind,
        source::SourceLocation location
    ) noexcept;

private:
    IrDeclarationKind kind_;
    source::SourceLocation location_;
};

// Represents one lowered top-level variable.
class IrVariableDeclaration final : public IrDeclaration {
public:
    // Creates a lowered source variable declaration.
    IrVariableDeclaration(
        std::string name,
        types::SemanticType type,
        std::unique_ptr<IrExpression> initializer,
        source::SourceLocation location
    );

    // Returns the variable name.
    [[nodiscard]] const std::string& getName() const noexcept;

    // Returns the variable type.
    [[nodiscard]] const types::SemanticType& getType() const noexcept;

    // Returns the lowered initializer.
    [[nodiscard]] const IrExpression& getInitializer() const noexcept;

private:
    std::string name_;
    types::SemanticType type_;
    std::unique_ptr<IrExpression> initializer_;
};

// Stores one lowered function parameter.
class IrParameter final {
public:
    // Creates a lowered function parameter.
    IrParameter(
        std::string name,
        types::SemanticType type,
        source::SourceLocation location
    );

    // Returns the parameter name.
    [[nodiscard]] const std::string& getName() const noexcept;

    // Returns the parameter type.
    [[nodiscard]] const types::SemanticType& getType() const noexcept;

    // Returns the parameter source location.
    [[nodiscard]] const source::SourceLocation& getLocation() const noexcept;

private:
    std::string name_;
    types::SemanticType type_;
    source::SourceLocation location_;
};

// Represents one lowered function with policy, signature, and instructions.
class IrFunctionDeclaration final : public IrDeclaration {
public:
    // Creates a lowered function declaration.
    IrFunctionDeclaration(
        std::string name,
        IrExecutionPolicy executionPolicy,
        std::vector<IrParameter> parameters,
        types::SemanticType returnType,
        std::vector<std::unique_ptr<IrStatement>> statements,
        source::SourceLocation location
    );

    // Returns the function name.
    [[nodiscard]] const std::string& getName() const noexcept;

    // Returns the function execution policy.
    [[nodiscard]] IrExecutionPolicy getExecutionPolicy() const noexcept;

    // Returns the ordered function parameters.
    [[nodiscard]] const std::vector<IrParameter>& getParameters() const noexcept;

    // Returns the logical function result type.
    [[nodiscard]] const types::SemanticType& getReturnType() const noexcept;

    // Returns the ordered lowered function instructions.
    [[nodiscard]] const std::vector<std::unique_ptr<IrStatement>>&
    getStatements() const noexcept;

private:
    std::string name_;
    IrExecutionPolicy executionPolicy_;
    std::vector<IrParameter> parameters_;
    types::SemanticType returnType_;
    std::vector<std::unique_ptr<IrStatement>> statements_;
};

// Stores one lowered model field in source order.
class IrModelField final {
public:
    // Creates a lowered model field.
    IrModelField(
        std::string name,
        types::SemanticType type,
        source::SourceLocation location
    );

    // Returns the field name.
    [[nodiscard]] const std::string& getName() const noexcept;

    // Returns the field type.
    [[nodiscard]] const types::SemanticType& getType() const noexcept;

    // Returns the field source location.
    [[nodiscard]] const source::SourceLocation& getLocation() const noexcept;

private:
    std::string name_;
    types::SemanticType type_;
    source::SourceLocation location_;
};

// Represents one lowered model schema.
class IrModelDeclaration final : public IrDeclaration {
public:
    // Creates a lowered model declaration.
    IrModelDeclaration(
        std::string name,
        std::vector<IrModelField> fields,
        source::SourceLocation location
    );

    // Returns the model name.
    [[nodiscard]] const std::string& getName() const noexcept;

    // Returns the ordered lowered model fields.
    [[nodiscard]] const std::vector<IrModelField>& getFields() const noexcept;

private:
    std::string name_;
    std::vector<IrModelField> fields_;
};

// Stores one lowered configuration entry.
class IrConfigEntry final {
public:
    // Creates a lowered config entry.
    IrConfigEntry(
        std::string name,
        types::SemanticType type,
        std::unique_ptr<IrExpression> value,
        source::SourceLocation location
    );

    // Returns the config key.
    [[nodiscard]] const std::string& getName() const noexcept;

    // Returns the config value type.
    [[nodiscard]] const types::SemanticType& getType() const noexcept;

    // Returns the lowered config value.
    [[nodiscard]] const IrExpression& getValue() const noexcept;

    // Returns the config entry source location.
    [[nodiscard]] const source::SourceLocation& getLocation() const noexcept;

private:
    std::string name_;
    types::SemanticType type_;
    std::unique_ptr<IrExpression> value_;
    source::SourceLocation location_;
};

// Represents one lowered configuration block.
class IrConfigDeclaration final : public IrDeclaration {
public:
    // Creates a lowered config declaration.
    IrConfigDeclaration(
        std::vector<IrConfigEntry> entries,
        source::SourceLocation location
    );

    // Returns the ordered lowered config entries.
    [[nodiscard]] const std::vector<IrConfigEntry>& getEntries() const noexcept;

private:
    std::vector<IrConfigEntry> entries_;
};

}
