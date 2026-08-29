#pragma once

#include <memory>
#include <vector>

#include "crossa/compiler/generators/kotlin/KotlinExpressionEmitter.h"
#include "crossa/compiler/generators/kotlin/KotlinSourceWriter.h"
#include "crossa/compiler/generators/kotlin/KotlinTypeMapper.h"
#include "crossa/compiler/ir/IrStatement.h"

namespace crossa::compiler::generators::kotlin {

// Emits Kotlin statements and structured conditional blocks from typed IR.
class KotlinStatementEmitter final {
public:
    // Creates a statement emitter over shared writer, type, and expression helpers.
    KotlinStatementEmitter(
        KotlinSourceWriter& writer,
        const KotlinTypeMapper& typeMapper,
        const KotlinExpressionEmitter& expressionEmitter,
        const KotlinIdentifierEscaper& identifierEscaper
    ) noexcept;

    // Emits each statement in canonical IR order.
    void emitStatements(
        const std::vector<std::unique_ptr<ir::IrStatement>>& statements
    ) const;

private:
    // Emits one typed IR statement.
    void emitStatement(const ir::IrStatement& statement) const;

    // Emits one conditional and its optional else-if or else branch.
    void emitIfStatement(
        const ir::IrIfStatement& statement,
        bool isElseIf
    ) const;

    // Raises a deterministic internal invariant diagnostic for malformed IR.
    [[noreturn]] static void failInvariant(const char* message);

    KotlinSourceWriter& writer_;
    const KotlinTypeMapper& typeMapper_;
    const KotlinExpressionEmitter& expressionEmitter_;
    const KotlinIdentifierEscaper& identifierEscaper_;
};

}
