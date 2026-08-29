#include "crossa/compiler/generators/kotlin/KotlinStatementEmitter.h"

#include <stdexcept>

using namespace std;

namespace crossa::compiler::generators::kotlin {

    // Creates a statement emitter over shared writer, type, and expression helpers.
    KotlinStatementEmitter::KotlinStatementEmitter(
        KotlinSourceWriter& writer,
        const KotlinTypeMapper& typeMapper,
        const KotlinExpressionEmitter& expressionEmitter,
        const KotlinIdentifierEscaper& identifierEscaper
    ) noexcept
        : writer_(writer),
          typeMapper_(typeMapper),
          expressionEmitter_(expressionEmitter),
          identifierEscaper_(identifierEscaper) {}

    // Emits each statement in canonical IR order.
    void KotlinStatementEmitter::emitStatements(
        const vector<unique_ptr<ir::IrStatement>>& statements
    ) const {
        for (const unique_ptr<ir::IrStatement>& statement : statements) {
            emitStatement(*statement);
        }
    }

    // Emits one typed IR statement.
    void KotlinStatementEmitter::emitStatement(
        const ir::IrStatement& statement
    ) const {
        switch (statement.getKind()) {
            case ir::IrStatementKind::Return:
                writer_.writeLine(
                    "return " + expressionEmitter_.emit(
                        static_cast<const ir::IrReturnStatement&>(statement)
                            .getExpression()
                    )
                );
                return;
            case ir::IrStatementKind::Evaluate:
                writer_.writeLine(expressionEmitter_.emit(
                    static_cast<const ir::IrEvaluateStatement&>(statement)
                        .getExpression()
                ));
                return;
            case ir::IrStatementKind::Local: {
                const auto& local = static_cast<const ir::IrLocalStatement&>(
                    statement
                );
                if (typeMapper_.isUnit(local.getType())) {
                    failInvariant("Local declaration cannot have Unit type.");
                }
                writer_.writeLine(
                    "val " + identifierEscaper_.escape(local.getName()) +
                    ": " + typeMapper_.mapValueType(local.getType()) + " = " +
                    expressionEmitter_.emit(local.getInitializer())
                );
                return;
            }
            case ir::IrStatementKind::If:
                emitIfStatement(
                    static_cast<const ir::IrIfStatement&>(statement),
                    false
                );
                return;
        }

        failInvariant("Unknown IR statement kind.");
    }

    // Emits one conditional and its optional else-if or else branch.
    void KotlinStatementEmitter::emitIfStatement(
        const ir::IrIfStatement& statement,
        bool isElseIf
    ) const {
        const string header = string(isElseIf ? "} else if (" : "if (") +
            expressionEmitter_.emit(statement.getCondition()) + ")";
        writer_.beginBlock(header);
        emitStatements(statement.getThenStatements());
        writer_.dedent();

        const vector<unique_ptr<ir::IrStatement>>* elseStatements =
            statement.getElseStatements();
        if (elseStatements == nullptr) {
            writer_.writeLine("}");
            return;
        }

        if (elseStatements->size() == 1 &&
            elseStatements->front()->getKind() == ir::IrStatementKind::If) {
            emitIfStatement(
                static_cast<const ir::IrIfStatement&>(*elseStatements->front()),
                true
            );
            return;
        }

        writer_.writeLine("} else {");
        writer_.indent();
        emitStatements(*elseStatements);
        writer_.endBlock();
    }

    // Raises a deterministic internal invariant diagnostic for malformed IR.
    [[noreturn]] void KotlinStatementEmitter::failInvariant(
        const char* message
    ) {
        throw runtime_error(
            string("Kotlin generation invariant failed: ") + message
        );
    }

}
