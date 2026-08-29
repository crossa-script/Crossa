#pragma once

#include <memory>

#include "crossa/compiler/ir/Program.h"
#include "crossa/compiler/semantic/TypedDeclaration.h"
#include "crossa/compiler/semantic/TypedExpression.h"
#include "crossa/compiler/semantic/TypedSourceUnit.h"
#include "crossa/compiler/semantic/TypedStatement.h"

namespace crossa::compiler::ir {

// Lowers validated semantic nodes into platform-neutral Crossa IR.
// lower() accepts only typed input, so generators and runtime need no AST lookup.
class IrLowerer final {
public:
    // Builds a complete IR program from one typed semantic source unit.
    [[nodiscard]] static Program lower(
        const semantic::TypedSourceUnit& sourceUnit
    );

private:
    // Lowers one typed declaration into a platform-neutral IR declaration.
    [[nodiscard]] static std::unique_ptr<IrDeclaration> lowerDeclaration(
        const semantic::TypedDeclaration& declaration
    );

    // Lowers one typed source variable declaration.
    [[nodiscard]] static std::unique_ptr<IrDeclaration>
    lowerVariableDeclaration(
        const semantic::TypedVariableDeclaration& declaration
    );

    // Lowers one typed model declaration and its ordered fields.
    [[nodiscard]] static std::unique_ptr<IrDeclaration> lowerModelDeclaration(
        const semantic::TypedModelDeclaration& declaration
    );

    // Lowers one typed function declaration and its body instructions.
    [[nodiscard]] static std::unique_ptr<IrDeclaration>
    lowerFunctionDeclaration(
        const semantic::TypedFunctionDeclaration& declaration
    );

    // Lowers one typed configuration declaration.
    [[nodiscard]] static std::unique_ptr<IrDeclaration>
    lowerConfigDeclaration(
        const semantic::TypedConfigDeclaration& declaration
    );

    // Lowers one typed top-level executable expression.
    [[nodiscard]] static std::unique_ptr<IrDeclaration>
    lowerExpressionDeclaration(
        const semantic::TypedExpressionDeclaration& declaration
    );

    // Lowers one typed statement into an IR instruction.
    [[nodiscard]] static std::unique_ptr<IrStatement> lowerStatement(
        const semantic::TypedStatement& statement
    );

    // Lowers one typed expression recursively into IR instructions.
    [[nodiscard]] static std::unique_ptr<IrExpression> lowerExpression(
        const semantic::TypedExpression& expression
    );

    // Maps a semantic symbol owner to its IR symbol owner.
    [[nodiscard]] static IrSymbolKind lowerSymbolKind(
        semantic::ValueSymbolKind symbolKind
    ) noexcept;

    // Maps a semantic execution policy to its IR scheduling policy.
    [[nodiscard]] static IrExecutionPolicy lowerExecutionPolicy(
        semantic::SemanticExecutionPolicy executionPolicy
    ) noexcept;

    // Maps a typed unary operation to an IR arithmetic operation.
    [[nodiscard]] static IrArithmeticOperator lowerUnaryOperator(
        semantic::TypedUnaryOperator operation
    ) noexcept;

    // Maps a typed binary operation to an IR arithmetic operation.
    [[nodiscard]] static IrArithmeticOperator lowerBinaryOperator(
        semantic::TypedBinaryOperator operation
    ) noexcept;

    // Raises an internal error if a validated node cannot be lowered.
    [[noreturn]] static void fail(const char* message);
};

}
