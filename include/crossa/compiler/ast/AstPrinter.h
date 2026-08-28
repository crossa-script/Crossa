#pragma once

#include <string>
#include <vector>

#include "crossa/compiler/ast/Declaration.h"
#include "crossa/compiler/ast/SourceUnit.h"
#include "crossa/compiler/ast/TypeReference.h"

namespace crossa::compiler::ast {

// Produces concise readable summaries of parsed AST declarations.
// summarize() exposes models, functions, annotations, and variables in debug mode.
class AstPrinter final {
public:
    // Returns one readable summary line for each top-level declaration.
    [[nodiscard]] static std::vector<std::string> summarize(
        const SourceUnit& sourceUnit
    );

private:
    // Returns a readable summary for one declaration.
    [[nodiscard]] static std::string summarizeDeclaration(
        const Declaration& declaration
    );

    // Returns a readable representation of one type reference.
    [[nodiscard]] static std::string formatType(const TypeReference& type);

    // Returns the readable name of one execution policy.
    [[nodiscard]] static std::string formatExecutionPolicy(
        ExecutionPolicy executionPolicy
    );
};

}
