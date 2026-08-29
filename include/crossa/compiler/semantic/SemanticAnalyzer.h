#pragma once

#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "crossa/compiler/ast/Declaration.h"
#include "crossa/compiler/ast/CrossaRequestExpression.h"
#include "crossa/compiler/ast/Expression.h"
#include "crossa/compiler/ast/JsonExpression.h"
#include "crossa/compiler/ast/SourceUnit.h"
#include "crossa/compiler/ast/Statement.h"
#include "crossa/compiler/ast/TypeReference.h"
#include "crossa/compiler/semantic/SemanticScope.h"
#include "crossa/compiler/semantic/TypedDeclaration.h"
#include "crossa/compiler/semantic/TypedCrossaRequestExpression.h"
#include "crossa/compiler/semantic/TypedExpression.h"
#include "crossa/compiler/semantic/TypedJsonExpression.h"
#include "crossa/compiler/semantic/TypedSourceUnit.h"
#include "crossa/compiler/semantic/TypedStatement.h"
#include "crossa/compiler/source/SourceFile.h"
#include "crossa/compiler/source/SourceLocation.h"
#include "crossa/compiler/types/SemanticType.h"

namespace crossa::compiler::semantic {

// Validates one syntax-only AST and produces its independent typed model.
// analyze() registers declarations first, then resolves every typed body.
class SemanticAnalyzer final {
public:
    // Creates an analyzer over one AST and its originating source file.
    SemanticAnalyzer(
        const ast::SourceUnit& sourceUnit,
        const source::SourceFile& sourceFile
    ) noexcept;

    // Validates the complete AST and returns the typed semantic source unit.
    [[nodiscard]] TypedSourceUnit analyze();

private:
    // Stores one pre-resolved callable signature for call validation.
    class FunctionSignature final {
    public:
        // Creates a function signature from its ordered argument and result types.
        FunctionSignature(
            std::vector<types::SemanticType> parameterTypes,
            types::SemanticType returnType
        );

        // Returns the ordered parameter types.
        [[nodiscard]] const std::vector<types::SemanticType>&
        getParameterTypes() const noexcept;

        // Returns the logical result type or Unit.
        [[nodiscard]] const types::SemanticType& getReturnType() const noexcept;

    private:
        std::vector<types::SemanticType> parameterTypes_;
        types::SemanticType returnType_;
    };

    // Validates reserved config-file structure before symbol registration.
    void validateSourceUnitShape() const;

    // Registers every top-level name and model type before type resolution.
    void registerTopLevelNames();

    // Resolves and registers all callable signatures before body analysis.
    void registerFunctionSignatures();

    // Resolves and registers all source variables in the global value scope.
    void registerSourceVariables();

    // Converts one AST declaration into a validated typed declaration.
    [[nodiscard]] std::unique_ptr<TypedDeclaration> analyzeDeclaration(
        const ast::Declaration& declaration
    );

    // Validates and converts one source variable declaration.
    [[nodiscard]] std::unique_ptr<TypedDeclaration> analyzeVariableDeclaration(
        const ast::VariableDeclaration& declaration
    );

    // Validates and converts one model declaration.
    [[nodiscard]] std::unique_ptr<TypedDeclaration> analyzeModelDeclaration(
        const ast::ModelDeclaration& declaration
    );

    // Validates and converts one function declaration and its lexical scope.
    [[nodiscard]] std::unique_ptr<TypedDeclaration> analyzeFunctionDeclaration(
        const ast::FunctionDeclaration& declaration
    );

    // Validates and converts one config declaration.
    [[nodiscard]] std::unique_ptr<TypedDeclaration> analyzeConfigDeclaration(
        const ast::ConfigDeclaration& declaration
    );

    // Validates and converts one top-level executable expression.
    [[nodiscard]] std::unique_ptr<TypedDeclaration>
    analyzeExpressionDeclaration(
        const ast::ExpressionDeclaration& declaration
    );

    // Validates and converts one function-body statement.
    [[nodiscard]] std::unique_ptr<TypedStatement> analyzeStatement(
        const ast::Statement& statement,
        SemanticScope& scope,
        const types::SemanticType& returnType,
        bool& hasReturn
    );

    // Validates and converts one local variable statement.
    [[nodiscard]] std::unique_ptr<TypedStatement> analyzeVariableStatement(
        const ast::VariableStatement& statement,
        SemanticScope& scope
    );

    // Resolves and converts one AST expression in the provided lexical scope.
    [[nodiscard]] std::unique_ptr<TypedExpression> analyzeExpression(
        const ast::Expression& expression,
        const SemanticScope& scope
    );

    // Resolves one expression with an optional contextual expected type.
    [[nodiscard]] std::unique_ptr<TypedExpression> analyzeExpression(
        const ast::Expression& expression,
        const SemanticScope& scope,
        const types::SemanticType* expectedType
    );

    // Resolves one identifier expression to a visible value symbol.
    [[nodiscard]] std::unique_ptr<TypedExpression> analyzeIdentifierExpression(
        const ast::IdentifierExpression& expression,
        const SemanticScope& scope
    );

    // Resolves every interpolation segment in one string expression.
    [[nodiscard]] std::unique_ptr<TypedExpression> analyzeStringExpression(
        const ast::StringLiteralExpression& expression,
        const SemanticScope& scope
    );

    // Validates one function or print builtin call.
    [[nodiscard]] std::unique_ptr<TypedExpression> analyzeCallExpression(
        const ast::CallExpression& expression,
        const SemanticScope& scope
    );

    // Validates one unary arithmetic expression.
    [[nodiscard]] std::unique_ptr<TypedExpression> analyzeUnaryExpression(
        const ast::UnaryExpression& expression,
        const SemanticScope& scope,
        const types::SemanticType* expectedType
    );

    // Validates one binary arithmetic expression.
    [[nodiscard]] std::unique_ptr<TypedExpression> analyzeBinaryExpression(
        const ast::BinaryExpression& expression,
        const SemanticScope& scope,
        const types::SemanticType* expectedType
    );

    // Validates one JSON object and recursively types every field value.
    [[nodiscard]] std::unique_ptr<TypedExpression> analyzeJsonObjectExpression(
        const ast::JsonObjectExpression& expression,
        const SemanticScope& scope
    );

    // Validates one JSON array and recursively types every item.
    [[nodiscard]] std::unique_ptr<TypedExpression> analyzeJsonArrayExpression(
        const ast::JsonArrayExpression& expression,
        const SemanticScope& scope
    );

    // Validates a direct request expression against its function result type.
    [[nodiscard]] std::unique_ptr<TypedExpression> analyzeCrossaRequestExpression(
        const ast::CrossaRequestExpression& expression,
        const SemanticScope& scope,
        const types::SemanticType& responseType
    );

    // Resolves URL interpolation including explicit path-variable aliases.
    [[nodiscard]] std::unique_ptr<TypedExpression> analyzeRequestUrl(
        const ast::Expression& expression,
        const ast::JsonObjectExpression* pathVariables,
        const SemanticScope& scope
    );

    // Ensures a request map is a JSON object containing scalar values.
    void validateRequestMap(
        const TypedExpression& expression,
        const std::string& fieldName
    ) const;

    // Returns one named request entry or null when it is absent.
    [[nodiscard]] static const ast::CrossaRequestEntry* findRequestEntry(
        const ast::CrossaRequestExpression& expression,
        const std::string& name
    ) noexcept;

    // Converts an AST HTTP method into its semantic request method.
    [[nodiscard]] static SemanticHttpMethod resolveHttpMethod(
        ast::HttpMethod method
    ) noexcept;

    // Resolves a syntax type reference into a complete semantic type.
    [[nodiscard]] types::SemanticType resolveType(
        const ast::TypeReference& typeReference
    ) const;

    // Converts an AST execution annotation into its semantic policy.
    [[nodiscard]] static SemanticExecutionPolicy resolveExecutionPolicy(
        ast::ExecutionPolicy executionPolicy
    ) noexcept;

    // Returns the stable global name represented by one declaration.
    [[nodiscard]] static const std::string* getDeclarationName(
        const ast::Declaration& declaration
    ) noexcept;

    // Returns the expected type for one supported config key.
    [[nodiscard]] static std::optional<types::SemanticType> getConfigType(
        const std::string& name
    );

    // Returns whether this type can participate in arithmetic expressions.
    [[nodiscard]] static bool isNumericType(
        const types::SemanticType& type
    ) noexcept;

    // Throws a deterministic source-aware semantic diagnostic.
    [[noreturn]] void fail(
        const source::SourceLocation& location,
        const std::string& code,
        const std::string& message
    ) const;

    const ast::SourceUnit& sourceUnit_;
    const source::SourceFile& sourceFile_;
    std::unordered_set<std::string> topLevelNames_;
    std::unordered_set<std::string> modelNames_;
    std::unordered_map<std::string, FunctionSignature> functionSignatures_;
    SemanticScope globalScope_;
};

}
