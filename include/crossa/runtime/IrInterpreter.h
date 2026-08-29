#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "crossa/compiler/ir/IrDeclaration.h"
#include "crossa/compiler/ir/IrExpression.h"
#include "crossa/compiler/ir/IrStatement.h"
#include "crossa/compiler/ir/Program.h"
#include "crossa/compiler/types/SemanticType.h"
#include "crossa/runtime/ExecutionFrame.h"
#include "crossa/runtime/RuntimeValue.h"
#include "crossa/utils/Log.h"

namespace crossa::runtime {

// Interprets the executable scalar subset of platform-neutral Crossa IR.
// It owns global values and creates a fresh frame for every function call.
class IrInterpreter final {
public:
    // Creates an interpreter over one immutable IR program and logger.
    IrInterpreter(
        const compiler::ir::Program& program,
        const utils::Log& log
    ) noexcept;

    // Initializes globals and executes top-level calls in source order.
    void execute();

private:
    // Indexes function declarations for deterministic name-based calls.
    void indexFunctions();

    // Evaluates all top-level variable initializers in declaration order.
    void initializeGlobals();

    // Evaluates each top-level executable expression in source order.
    void executeTopLevelExpressions();

    // Invokes a function with already evaluated runtime arguments.
    [[nodiscard]] RuntimeValue invokeFunction(
        const compiler::ir::IrFunctionDeclaration& function,
        std::vector<RuntimeValue> arguments,
        std::size_t callDepth
    );

    // Executes a function's ordered statements until it returns.
    [[nodiscard]] RuntimeValue executeFunctionBody(
        const compiler::ir::IrFunctionDeclaration& function,
        ExecutionFrame& frame,
        std::size_t callDepth
    );

    // Executes one IR statement in the current function frame.
    [[nodiscard]] std::optional<RuntimeValue> executeStatement(
        const compiler::ir::IrStatement& statement,
        ExecutionFrame& frame,
        std::size_t callDepth
    );

    // Evaluates one IR expression in the current runtime frame.
    [[nodiscard]] RuntimeValue evaluate(
        const compiler::ir::IrExpression& expression,
        const ExecutionFrame& frame,
        std::size_t callDepth
    );

    // Resolves one symbol read against globals or the active frame.
    [[nodiscard]] const RuntimeValue& resolveSymbol(
        const std::string& name,
        compiler::ir::IrSymbolKind symbolKind,
        const ExecutionFrame& frame
    ) const;

    // Evaluates a binary arithmetic operation with overflow protection.
    [[nodiscard]] static std::int64_t evaluateArithmetic(
        std::int64_t left,
        compiler::ir::IrArithmeticOperator operation,
        std::int64_t right
    );

    // Raises a deterministic runtime execution failure.
    [[noreturn]] static void fail(const std::string& message);

    const compiler::ir::Program& program_;
    const utils::Log& log_;
    ExecutionFrame globals_;
    std::unordered_map<std::string, const compiler::ir::IrFunctionDeclaration*>
        functions_;
};

}
