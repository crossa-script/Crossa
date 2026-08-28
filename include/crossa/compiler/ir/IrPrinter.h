#pragma once

#include <string>
#include <vector>

#include "crossa/compiler/ir/IrDeclaration.h"
#include "crossa/compiler/ir/Program.h"

namespace crossa::compiler::ir {

// Produces readable summaries of lowered Crossa IR declarations.
class IrPrinter final {
public:
    // Returns one or more readable lines for the complete IR program.
    [[nodiscard]] static std::vector<std::string> summarize(
        const Program& program
    );

private:
    // Formats one IR declaration and its nested instructions.
    [[nodiscard]] static std::vector<std::string> summarizeDeclaration(
        const IrDeclaration& declaration
    );

    // Formats one IR statement at the requested indentation level.
    [[nodiscard]] static std::string formatStatement(
        const IrStatement& statement,
        const std::string& indentation
    );

    // Formats one IR expression as a compact instruction expression.
    [[nodiscard]] static std::string formatExpression(
        const IrExpression& expression
    );

    // Formats one IR scheduling policy.
    [[nodiscard]] static std::string formatExecutionPolicy(
        IrExecutionPolicy executionPolicy
    );

    // Formats one IR arithmetic operation.
    [[nodiscard]] static std::string formatArithmeticOperator(
        IrArithmeticOperator operation
    );
};

}
