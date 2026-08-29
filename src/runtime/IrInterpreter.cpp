#include "crossa/runtime/IrInterpreter.h"

#include <charconv>
#include <limits>
#include <stdexcept>
#include <utility>

#include "crossa/utils/PrintUtils.h"

using namespace std;

namespace crossa::runtime {

    // Creates an interpreter over one immutable IR program and logger.
    IrInterpreter::IrInterpreter(
        const compiler::ir::Program& program,
        const utils::Log& log
    ) noexcept
        : program_(program), log_(log) {
        indexFunctions();
    }

    // Initializes globals and executes top-level calls in source order.
    void IrInterpreter::execute() {
        log_.debug("IR interpreter started");
        initializeGlobals();
        executeTopLevelExpressions();
        log_.debug("IR interpreter completed");
    }

    // Indexes function declarations for deterministic name-based calls.
    void IrInterpreter::indexFunctions() {
        for (const unique_ptr<compiler::ir::IrDeclaration>& declaration :
             program_.getDeclarations()) {
            if (declaration->getKind() != compiler::ir::IrDeclarationKind::Function) {
                continue;
            }

            const auto& function =
                static_cast<const compiler::ir::IrFunctionDeclaration&>(
                    *declaration
                );
            functions_.emplace(function.getName(), &function);
        }
    }

    // Evaluates all top-level variable initializers in declaration order.
    void IrInterpreter::initializeGlobals() {
        for (const unique_ptr<compiler::ir::IrDeclaration>& declaration :
             program_.getDeclarations()) {
            if (declaration->getKind() != compiler::ir::IrDeclarationKind::Variable) {
                continue;
            }

            const auto& variable =
                static_cast<const compiler::ir::IrVariableDeclaration&>(
                    *declaration
                );
            const RuntimeValue value = evaluate(
                variable.getInitializer(),
                ExecutionFrame(),
                0
            );
            if (value.getKind() == RuntimeValueKind::Unit) {
                fail(
                    "Global variable '" + variable.getName() +
                    "' cannot be initialized with Unit."
                );
            }
            if (!globals_.declare(variable.getName(), value)) {
                fail("Duplicate runtime global '" + variable.getName() + "'.");
            }
        }
    }

    // Evaluates each top-level executable expression in source order.
    void IrInterpreter::executeTopLevelExpressions() {
        for (const unique_ptr<compiler::ir::IrDeclaration>& declaration :
             program_.getDeclarations()) {
            if (declaration->getKind() !=
                compiler::ir::IrDeclarationKind::Expression) {
                continue;
            }

            const auto& expression =
                static_cast<const compiler::ir::IrExpressionDeclaration&>(
                    *declaration
                );
            (void)evaluate(expression.getExpression(), ExecutionFrame(), 0);
        }
    }

    // Invokes a function with already evaluated runtime arguments.
    RuntimeValue IrInterpreter::invokeFunction(
        const compiler::ir::IrFunctionDeclaration& function,
        vector<RuntimeValue> arguments,
        size_t callDepth
    ) {
        constexpr size_t MaximumCallDepth = 1024;
        if (callDepth >= MaximumCallDepth) {
            fail("Maximum function call depth exceeded.");
        }

        ExecutionFrame frame;
        for (size_t index = 0; index < arguments.size(); ++index) {
            if (!frame.declare(function.getParameters()[index].getName(),
                               std::move(arguments[index]))) {
                fail("Duplicate runtime parameter.");
            }
        }

        log_.debug("Invoking function: " + function.getName());
        return executeFunctionBody(function, frame, callDepth);
    }

    // Executes a function's ordered statements until it returns.
    RuntimeValue IrInterpreter::executeFunctionBody(
        const compiler::ir::IrFunctionDeclaration& function,
        ExecutionFrame& frame,
        size_t callDepth
    ) {
        for (const unique_ptr<compiler::ir::IrStatement>& statement :
             function.getStatements()) {
            const optional<RuntimeValue> result = executeStatement(
                *statement,
                frame,
                callDepth
            );
            if (result.has_value()) {
                return *result;
            }
        }

        return RuntimeValue::createUnit();
    }

    // Executes one IR statement in the current function frame.
    optional<RuntimeValue> IrInterpreter::executeStatement(
        const compiler::ir::IrStatement& statement,
        ExecutionFrame& frame,
        size_t callDepth
    ) {
        switch (statement.getKind()) {
            case compiler::ir::IrStatementKind::Return:
                return evaluate(
                    static_cast<const compiler::ir::IrReturnStatement&>(
                        statement
                    ).getExpression(),
                    frame,
                    callDepth
                );
            case compiler::ir::IrStatementKind::Evaluate:
                (void)evaluate(
                    static_cast<const compiler::ir::IrEvaluateStatement&>(
                        statement
                    ).getExpression(),
                    frame,
                    callDepth
                );
                return nullopt;
            case compiler::ir::IrStatementKind::Local: {
                const auto& local =
                    static_cast<const compiler::ir::IrLocalStatement&>(statement);
                const RuntimeValue value = evaluate(
                    local.getInitializer(),
                    frame,
                    callDepth
                );
                if (!frame.declare(local.getName(), value)) {
                    fail("Duplicate runtime local '" + local.getName() + "'.");
                }
                return nullopt;
            }
        }

        fail("Unknown IR statement kind.");
    }

    // Evaluates one IR expression in the current runtime frame.
    RuntimeValue IrInterpreter::evaluate(
        const compiler::ir::IrExpression& expression,
        const ExecutionFrame& frame,
        size_t callDepth
    ) {
        switch (expression.getKind()) {
            case compiler::ir::IrExpressionKind::ReadSymbol: {
                const auto& read =
                    static_cast<const compiler::ir::IrReadSymbolExpression&>(
                        expression
                    );
                return resolveSymbol(read.getName(), read.getSymbolKind(), frame);
            }
            case compiler::ir::IrExpressionKind::IntegerConstant: {
                const auto& literal =
                    static_cast<const compiler::ir::IrIntegerConstantExpression&>(
                        expression
                    );
                int64_t value = 0;
                const string& digits = literal.getValue();
                const auto result = from_chars(
                    digits.data(),
                    digits.data() + digits.size(),
                    value
                );
                if (result.ec != errc{} || result.ptr != digits.data() + digits.size()) {
                    fail("Integer literal is outside the native Int range.");
                }
                return RuntimeValue::createInt(value);
            }
            case compiler::ir::IrExpressionKind::StringBuild: {
                const auto& stringBuild =
                    static_cast<const compiler::ir::IrStringBuildExpression&>(
                        expression
                    );
                string value;
                for (const compiler::ir::IrStringSegment& segment :
                     stringBuild.getSegments()) {
                    if (segment.getKind() ==
                        compiler::ir::IrStringSegmentKind::Literal) {
                        value += segment.getValue();
                        continue;
                    }
                    const compiler::ir::IrSymbolKind* symbolKind =
                        segment.getSymbolKind();
                    if (symbolKind == nullptr) {
                        fail("String symbol segment is missing its symbol kind.");
                    }
                    value += resolveSymbol(
                        segment.getValue(),
                        *symbolKind,
                        frame
                    ).format();
                }
                return RuntimeValue::createString(std::move(value));
            }
            case compiler::ir::IrExpressionKind::BooleanConstant:
                return RuntimeValue::createBool(
                    static_cast<
                        const compiler::ir::IrBooleanConstantExpression&
                    >(expression).getValue()
                );
            case compiler::ir::IrExpressionKind::Call: {
                const auto& call =
                    static_cast<const compiler::ir::IrCallExpression&>(expression);
                vector<RuntimeValue> arguments;
                arguments.reserve(call.getArguments().size());
                for (const unique_ptr<compiler::ir::IrExpression>& argument :
                     call.getArguments()) {
                    arguments.push_back(evaluate(*argument, frame, callDepth));
                }
                if (call.isBuiltin()) {
                    if (call.getCallee() != "print" || arguments.size() != 1) {
                        fail("Unsupported builtin call in IR.");
                    }
                    utils::PrintUtils::println(arguments.front().format());
                    return RuntimeValue::createUnit();
                }
                const auto functionIterator = functions_.find(call.getCallee());
                if (functionIterator == functions_.end()) {
                    fail("Unknown function '" + call.getCallee() + "'.");
                }
                return invokeFunction(
                    *functionIterator->second,
                    std::move(arguments),
                    callDepth + 1
                );
            }
            case compiler::ir::IrExpressionKind::Unary: {
                const auto& unary =
                    static_cast<const compiler::ir::IrUnaryExpression&>(expression);
                const int64_t value = evaluate(
                    unary.getOperand(),
                    frame,
                    callDepth
                ).getInt();
                return RuntimeValue::createInt(
                    evaluateArithmetic(
                        0,
                        unary.getOperator(),
                        value
                    )
                );
            }
            case compiler::ir::IrExpressionKind::Binary: {
                const auto& binary =
                    static_cast<const compiler::ir::IrBinaryExpression&>(expression);
                const int64_t left = evaluate(
                    binary.getLeft(),
                    frame,
                    callDepth
                ).getInt();
                const int64_t right = evaluate(
                    binary.getRight(),
                    frame,
                    callDepth
                ).getInt();
                return RuntimeValue::createInt(
                    evaluateArithmetic(left, binary.getOperator(), right)
                );
            }
        }

        fail("Unknown IR expression kind.");
    }

    // Resolves one symbol read against globals or the active frame.
    const RuntimeValue& IrInterpreter::resolveSymbol(
        const string& name,
        compiler::ir::IrSymbolKind symbolKind,
        const ExecutionFrame& frame
    ) const {
        const RuntimeValue* value = symbolKind ==
                compiler::ir::IrSymbolKind::SourceVariable
            ? globals_.resolve(name)
            : frame.resolve(name);
        if (value == nullptr) {
            fail("Runtime symbol '" + name + "' has no value.");
        }
        return *value;
    }

    // Evaluates a binary arithmetic operation with overflow protection.
    int64_t IrInterpreter::evaluateArithmetic(
        int64_t left,
        compiler::ir::IrArithmeticOperator operation,
        int64_t right
    ) {
        const int64_t minimum = numeric_limits<int64_t>::min();
        const int64_t maximum = numeric_limits<int64_t>::max();
        switch (operation) {
            case compiler::ir::IrArithmeticOperator::Add:
                if ((right > 0 && left > maximum - right) ||
                    (right < 0 && left < minimum - right)) {
                    fail("Int addition overflow.");
                }
                return left + right;
            case compiler::ir::IrArithmeticOperator::Subtract:
                if ((right < 0 && left > maximum + right) ||
                    (right > 0 && left < minimum + right)) {
                    fail("Int subtraction overflow.");
                }
                return left - right;
            case compiler::ir::IrArithmeticOperator::Multiply:
                if (left == 0 || right == 0) {
                    return 0;
                }
                if ((left == -1 && right == minimum) ||
                    (right == -1 && left == minimum)) {
                    fail("Int multiplication overflow.");
                }
                if (left > 0) {
                    if (right > 0 && left > maximum / right) {
                        fail("Int multiplication overflow.");
                    }
                    if (right < 0 && right < minimum / left) {
                        fail("Int multiplication overflow.");
                    }
                } else {
                    if (right > 0 && left < minimum / right) {
                        fail("Int multiplication overflow.");
                    }
                    if (right < 0 && left < maximum / right) {
                        fail("Int multiplication overflow.");
                    }
                }
                return left * right;
            case compiler::ir::IrArithmeticOperator::Divide:
                if (right == 0) {
                    fail("Division by zero.");
                }
                if (left == minimum && right == -1) {
                    fail("Int division overflow.");
                }
                return left / right;
            case compiler::ir::IrArithmeticOperator::Negate:
                if (right == minimum) {
                    fail("Int negation overflow.");
                }
                return -right;
        }

        fail("Unknown IR arithmetic operation.");
    }

    // Raises a deterministic runtime execution failure.
    [[noreturn]] void IrInterpreter::fail(const string& message) {
        throw runtime_error("Native IR execution failed: " + message);
    }

}
