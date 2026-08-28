#include "crossa/compiler/semantic/SemanticModelPrinter.h"

using namespace std;

namespace crossa::compiler::semantic {

    // Returns one readable summary for each typed declaration.
    vector<string> SemanticModelPrinter::summarize(
        const TypedSourceUnit& sourceUnit
    ) {
        vector<string> summaries;
        summaries.reserve(sourceUnit.getDeclarations().size());

        for (const unique_ptr<TypedDeclaration>& declaration :
             sourceUnit.getDeclarations()) {
            summaries.push_back(summarizeDeclaration(*declaration));
        }

        return summaries;
    }

    // Returns a readable summary for one typed declaration.
    string SemanticModelPrinter::summarizeDeclaration(
        const TypedDeclaration& declaration
    ) {
        switch (declaration.getKind()) {
            case TypedDeclarationKind::Variable: {
                const auto& variable =
                    static_cast<const TypedVariableDeclaration&>(declaration);
                return "Semantic Variable " + variable.getName() + ": " +
                       variable.getType().format();
            }
            case TypedDeclarationKind::Model: {
                const auto& model =
                    static_cast<const TypedModelDeclaration&>(declaration);
                return "Semantic Model " + model.getName() + " fields=" +
                       to_string(model.getFields().size());
            }
            case TypedDeclarationKind::Function: {
                const auto& function =
                    static_cast<const TypedFunctionDeclaration&>(declaration);
                return "Semantic Function " + function.getName() +
                       " policy=" +
                       formatExecutionPolicy(function.getExecutionPolicy()) +
                       " parameters=" +
                       to_string(function.getParameters().size()) +
                       " return=" + function.getReturnType().format() +
                       " statements=" +
                       to_string(function.getStatements().size());
            }
            case TypedDeclarationKind::Config: {
                const auto& config =
                    static_cast<const TypedConfigDeclaration&>(declaration);
                return "Semantic Config entries=" +
                       to_string(config.getEntries().size());
            }
        }

        return "Semantic Unknown declaration";
    }

    // Returns a stable readable name for one execution policy.
    string SemanticModelPrinter::formatExecutionPolicy(
        SemanticExecutionPolicy executionPolicy
    ) {
        switch (executionPolicy) {
            case SemanticExecutionPolicy::Sync:
                return "Sync";
            case SemanticExecutionPolicy::Async:
                return "Async";
            case SemanticExecutionPolicy::AsyncAfter:
                return "AsyncAfter";
        }

        return "Unknown";
    }

}
