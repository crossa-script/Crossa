#pragma once

#include <string>

#include "crossa/compiler/generators/kotlin/KotlinIdentifierEscaper.h"
#include "crossa/compiler/ir/IrExpression.h"

namespace crossa::compiler::generators::kotlin {

// Emits Kotlin expressions from typed IR while preserving the IR expression tree.
class KotlinExpressionEmitter final {
public:
    // Creates an emitter using the shared Kotlin identifier escaping rules.
    explicit KotlinExpressionEmitter(
        const KotlinIdentifierEscaper& identifierEscaper
    ) noexcept;

    // Emits one complete Kotlin expression from typed Crossa IR.
    [[nodiscard]] std::string emit(const ir::IrExpression& expression) const;

private:
    // Emits one expression and parenthesizes it when the parent requires it.
    [[nodiscard]] std::string emitWithParentPrecedence(
        const ir::IrExpression& expression,
        int parentPrecedence
    ) const;

    // Emits one direct Kotlin expression without parent-required parentheses.
    [[nodiscard]] std::string emitExpression(
        const ir::IrExpression& expression
    ) const;

    // Emits one resolved Kotlin function or supported builtin call.
    [[nodiscard]] std::string emitCall(
        const ir::IrCallExpression& expression
    ) const;

    // Emits one pre-resolved string construction expression.
    [[nodiscard]] std::string emitStringBuild(
        const ir::IrStringBuildExpression& expression
    ) const;

    // Escapes one Crossa string segment as a Kotlin string literal.
    [[nodiscard]] static std::string escapeStringLiteral(
        const std::string& value
    );

    // Returns the target precedence for one typed IR expression.
    [[nodiscard]] static int precedenceOf(
        const ir::IrExpression& expression
    );

    // Returns the target precedence for one binary IR operation.
    [[nodiscard]] static int precedenceOf(
        ir::IrArithmeticOperator operation
    );

    // Returns Kotlin syntax for one supported unary IR operation.
    [[nodiscard]] static const char* mapUnaryOperator(
        ir::IrArithmeticOperator operation
    );

    // Returns Kotlin syntax for one supported binary IR operation.
    [[nodiscard]] static const char* mapBinaryOperator(
        ir::IrArithmeticOperator operation
    );

    // Raises a deterministic unsupported-backend diagnostic for one IR feature.
    [[noreturn]] static void failUnsupported(const char* feature);

    // Raises a deterministic internal invariant diagnostic for malformed IR.
    [[noreturn]] static void failInvariant(const char* message);

    const KotlinIdentifierEscaper& identifierEscaper_;
};

}
