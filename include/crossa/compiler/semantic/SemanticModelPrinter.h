#pragma once

#include <string>
#include <vector>

#include "crossa/compiler/semantic/TypedDeclaration.h"
#include "crossa/compiler/semantic/TypedSourceUnit.h"

namespace crossa::compiler::semantic {

// Produces concise summaries of the validated typed semantic model.
class SemanticModelPrinter final {
public:
    // Returns one readable summary for each typed declaration.
    [[nodiscard]] static std::vector<std::string> summarize(
        const TypedSourceUnit& sourceUnit
    );

private:
    // Returns a readable summary for one typed declaration.
    [[nodiscard]] static std::string summarizeDeclaration(
        const TypedDeclaration& declaration
    );

    // Returns a stable readable name for one execution policy.
    [[nodiscard]] static std::string formatExecutionPolicy(
        SemanticExecutionPolicy executionPolicy
    );
};

}
