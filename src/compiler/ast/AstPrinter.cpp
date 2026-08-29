#include "crossa/compiler/ast/AstPrinter.h"

using namespace std;

namespace crossa::compiler::ast {

    // Returns one readable summary line for each top-level declaration.
    vector<string> AstPrinter::summarize(const SourceUnit& sourceUnit) {
        vector<string> summaries;
        summaries.reserve(sourceUnit.getDeclarations().size());

        for (const unique_ptr<Declaration>& declaration :
             sourceUnit.getDeclarations()) {
            summaries.push_back(summarizeDeclaration(*declaration));
        }

        return summaries;
    }

    // Returns a readable summary for one declaration.
    string AstPrinter::summarizeDeclaration(const Declaration& declaration) {
        switch (declaration.getKind()) {
            case DeclarationKind::Import: {
                const auto& import =
                    static_cast<const ImportDeclaration&>(declaration);
                return "AST Import " + import.getFilename();
            }
            case DeclarationKind::Variable: {
                const auto& variable =
                    static_cast<const VariableDeclaration&>(declaration);
                return "AST Variable " + variable.getName() + ": " +
                       formatType(variable.getType());
            }
            case DeclarationKind::Model: {
                const auto& model =
                    static_cast<const ModelDeclaration&>(declaration);
                return "AST Model " + model.getName() + " fields=" +
                       to_string(model.getFields().size());
            }
            case DeclarationKind::Function: {
                const auto& function =
                    static_cast<const FunctionDeclaration&>(declaration);
                const TypeReference* returnType = function.getReturnType();
                const string resultType =
                    returnType == nullptr ? "Unit" : formatType(*returnType);
                return "AST Function " + function.getName() +
                       " policy=" +
                       formatExecutionPolicy(function.getExecutionPolicy()) +
                       " parameters=" +
                       to_string(function.getParameters().size()) +
                       " return=" + resultType +
                       " statements=" +
                       to_string(function.getStatements().size());
            }
            case DeclarationKind::Config: {
                const auto& config =
                    static_cast<const ConfigDeclaration&>(declaration);
                return "AST Config entries=" +
                       to_string(config.getEntries().size());
            }
            case DeclarationKind::Expression:
                return "AST Top-level expression";
        }

        return "AST Unknown declaration";
    }

    // Returns a readable representation of one type reference.
    string AstPrinter::formatType(const TypeReference& type) {
        if (type.getKind() == TypeReferenceKind::Named) {
            return type.getName();
        }

        const TypeReference* elementType = type.getElementType();
        return elementType == nullptr
            ? "List<?>"
            : "List<" + formatType(*elementType) + ">";
    }

    // Returns the readable name of one execution policy.
    string AstPrinter::formatExecutionPolicy(
        ExecutionPolicy executionPolicy
    ) {
        switch (executionPolicy) {
            case ExecutionPolicy::None:
                return "None";
            case ExecutionPolicy::Sync:
                return "Sync";
            case ExecutionPolicy::Async:
                return "Async";
            case ExecutionPolicy::AsyncAfter:
                return "AsyncAfter";
        }

        return "Unknown";
    }

}
