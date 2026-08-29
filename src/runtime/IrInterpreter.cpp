#include "crossa/runtime/IrInterpreter.h"

#include <charconv>
#include <limits>
#include <stdexcept>
#include <utility>

#include "crossa/compiler/ir/IrCrossaRequestExpression.h"
#include "crossa/compiler/ir/IrJsonExpression.h"
#include "crossa/network/json/JsonSerializer.h"
#include "crossa/network/utils/UrlUtils.h"
#include "crossa/utils/PrintUtils.h"

using namespace std;

namespace crossa::runtime {

    // Creates an interpreter over one immutable IR program and logger.
    IrInterpreter::IrInterpreter(
        const compiler::ir::Program& program,
        const utils::Log& log,
        scheduler::TaskScheduler& scheduler,
        network::NetworkEngine& networkEngine,
        const network::NetworkConfiguration& networkConfiguration
    ) noexcept
        : program_(program),
          log_(log),
          scheduler_(scheduler),
          networkEngine_(networkEngine),
          responseDecoder_(
              program,
              networkConfiguration.getMaximumResponseBytes(),
              networkConfiguration.getMaximumJsonDepth()
          ) {
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
                return RuntimeValue::createString(
                    evaluateStringBuild(stringBuild, frame, false)
                );
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
                return invokeScheduledFunction(
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
            case compiler::ir::IrExpressionKind::JsonNumber:
                return RuntimeValue::createJson(
                    network::json::JsonValue::createNumber(
                        static_cast<
                            const compiler::ir::IrJsonNumberExpression&
                        >(expression).getValue()
                    )
                );
            case compiler::ir::IrExpressionKind::JsonNull:
                return RuntimeValue::createJson(
                    network::json::JsonValue::createNull()
                );
            case compiler::ir::IrExpressionKind::JsonObject: {
                const auto& object = static_cast<
                    const compiler::ir::IrJsonObjectExpression&
                >(expression);
                network::json::JsonValue::Object values;
                values.reserve(object.getEntries().size());
                for (const compiler::ir::IrJsonObjectEntry& entry :
                     object.getEntries()) {
                    values.emplace_back(
                        entry.getKey(),
                        toJsonValue(evaluate(
                            entry.getValue(),
                            frame,
                            callDepth
                        ))
                    );
                }
                return RuntimeValue::createJson(
                    network::json::JsonValue::createObject(std::move(values))
                );
            }
            case compiler::ir::IrExpressionKind::JsonArray: {
                const auto& array = static_cast<
                    const compiler::ir::IrJsonArrayExpression&
                >(expression);
                network::json::JsonValue::Array values;
                values.reserve(array.getValues().size());
                for (const unique_ptr<compiler::ir::IrExpression>& value :
                     array.getValues()) {
                    values.push_back(toJsonValue(evaluate(
                        *value,
                        frame,
                        callDepth
                    )));
                }
                return RuntimeValue::createJson(
                    network::json::JsonValue::createArray(std::move(values))
                );
            }
            case compiler::ir::IrExpressionKind::CrossaRequest:
                return evaluateRequest(
                    static_cast<
                        const compiler::ir::IrCrossaRequestExpression&
                    >(expression),
                    frame,
                    callDepth
                );
        }

        fail("Unknown IR expression kind.");
    }

    // Evaluates and executes one lowered native CrossaRequest plan.
    RuntimeValue IrInterpreter::evaluateRequest(
        const compiler::ir::IrCrossaRequestExpression& request,
        const ExecutionFrame& frame,
        size_t callDepth
    ) {
        const auto& urlExpression = static_cast<
            const compiler::ir::IrStringBuildExpression&
        >(request.getUrl());
        optional<string> body;
        if (request.getBody() != nullptr) {
            body = network::json::JsonSerializer::serialize(
                toJsonValue(evaluate(
                    *request.getBody(),
                    frame,
                    callDepth
                ))
            );
        }
        optional<int64_t> timeout;
        if (request.getTimeout() != nullptr) {
            timeout = evaluate(
                *request.getTimeout(),
                frame,
                callDepth
            ).getInt();
            if (*timeout <= 0 || *timeout > 600000) {
                fail("CrossaRequest timeout must be between 1 and 600000 ms.");
            }
        }

        network::request::RequestSpec spec(
            resolveHttpMethod(request.getMethod()),
            evaluateStringBuild(urlExpression, frame, true),
            evaluateHeaders(request.getHeaders(), frame, callDepth),
            evaluateHeaders(request.getCustomHeaders(), frame, callDepth),
            evaluateQueryParameters(
                request.getQueryParams(),
                frame,
                callDepth
            ),
            std::move(body),
            timeout
        );
        log_.debug("CrossaRequest execution entered native network engine");
        const network::response::HttpResponse response =
            networkEngine_.execute(spec);
        log_.debug("CrossaRequest response entered native decoder");
        return responseDecoder_.decode(
            response.getBody(),
            request.getType()
        );
    }

    // Evaluates one string build with optional URL component encoding.
    string IrInterpreter::evaluateStringBuild(
        const compiler::ir::IrStringBuildExpression& expression,
        const ExecutionFrame& frame,
        bool encodeSymbols
    ) const {
        string value;
        for (const compiler::ir::IrStringSegment& segment :
             expression.getSegments()) {
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
            string symbolValue = resolveSymbol(
                segment.getValue(),
                *symbolKind,
                frame
            ).format();
            value += encodeSymbols
                ? network::utils::UrlUtils::encodeComponent(symbolValue)
                : symbolValue;
        }
        return value;
    }

    // Evaluates one optional request map into HTTP headers.
    vector<network::HttpHeader> IrInterpreter::evaluateHeaders(
        const compiler::ir::IrExpression* expression,
        const ExecutionFrame& frame,
        size_t callDepth
    ) {
        if (expression == nullptr) {
            return {};
        }
        const RuntimeValue value = evaluate(*expression, frame, callDepth);
        if (value.getKind() != RuntimeValueKind::Json ||
            value.getJson().getKind() !=
                network::json::JsonValueKind::Object) {
            fail("CrossaRequest headers must evaluate to a JSON object.");
        }
        vector<network::HttpHeader> headers;
        headers.reserve(value.getJson().getObject().size());
        for (const auto& [name, headerValue] :
             value.getJson().getObject()) {
            headers.emplace_back(name, jsonScalarToString(headerValue));
        }
        return headers;
    }

    // Evaluates one optional request map into ordered query parameters.
    network::request::RequestSpec::QueryParameters
    IrInterpreter::evaluateQueryParameters(
        const compiler::ir::IrExpression* expression,
        const ExecutionFrame& frame,
        size_t callDepth
    ) {
        if (expression == nullptr) {
            return {};
        }
        const RuntimeValue value = evaluate(*expression, frame, callDepth);
        if (value.getKind() != RuntimeValueKind::Json ||
            value.getJson().getKind() !=
                network::json::JsonValueKind::Object) {
            fail("CrossaRequest queryParams must evaluate to a JSON object.");
        }
        network::request::RequestSpec::QueryParameters parameters;
        parameters.reserve(value.getJson().getObject().size());
        for (const auto& [name, parameterValue] :
             value.getJson().getObject()) {
            parameters.emplace_back(name, jsonScalarToString(parameterValue));
        }
        return parameters;
    }

    // Executes a function according to its lowered scheduling policy.
    RuntimeValue IrInterpreter::invokeScheduledFunction(
        const compiler::ir::IrFunctionDeclaration& function,
        vector<RuntimeValue> arguments,
        size_t callDepth
    ) {
        switch (function.getExecutionPolicy()) {
            case compiler::ir::IrExecutionPolicy::Sync:
                log_.debug("Function policy selected: @Sync inline");
                return invokeFunction(
                    function,
                    std::move(arguments),
                    callDepth
                );
            case compiler::ir::IrExecutionPolicy::Async:
                log_.debug("Function policy selected: @Async background");
                if (scheduler_.isWorkerThread()) {
                    (void)invokeFunction(
                        function,
                        std::move(arguments),
                        callDepth
                    );
                } else {
                    scheduler_.submitDetached(
                        [this, &function, arguments = std::move(arguments),
                         callDepth]() mutable {
                            try {
                                (void)invokeFunction(
                                    function,
                                    std::move(arguments),
                                    callDepth
                                );
                            } catch (const exception& error) {
                                log_.error(
                                    "@Async function '" +
                                    function.getName() + "' failed: " +
                                    error.what()
                                );
                            }
                        }
                    );
                }
                return RuntimeValue::createUnit();
            case compiler::ir::IrExecutionPolicy::AsyncAfter:
                log_.debug("Function policy selected: @AsyncAfter background");
                if (scheduler_.isWorkerThread()) {
                    return invokeFunction(
                        function,
                        std::move(arguments),
                        callDepth
                    );
                }
                return scheduler_.submit(
                    [this, &function, arguments = std::move(arguments),
                     callDepth]() mutable {
                        return invokeFunction(
                            function,
                            std::move(arguments),
                            callDepth
                        );
                    }
                ).get();
        }
        fail("Unknown function scheduling policy.");
    }

    // Converts one runtime scalar or Json value into an owned JSON value.
    network::json::JsonValue IrInterpreter::toJsonValue(
        const RuntimeValue& value
    ) {
        switch (value.getKind()) {
            case RuntimeValueKind::Int:
                return network::json::JsonValue::createNumber(
                    to_string(value.getInt())
                );
            case RuntimeValueKind::String:
                return network::json::JsonValue::createString(
                    value.getString()
                );
            case RuntimeValueKind::Bool:
                return network::json::JsonValue::createBoolean(
                    value.getBool()
                );
            case RuntimeValueKind::Json:
                return value.getJson();
            case RuntimeValueKind::Unit:
                fail("Unit cannot be encoded as JSON.");
        }
        fail("Unknown runtime value kind for JSON encoding.");
    }

    // Converts one scalar JSON value into request metadata text.
    string IrInterpreter::jsonScalarToString(
        const network::json::JsonValue& value
    ) {
        switch (value.getKind()) {
            case network::json::JsonValueKind::Boolean:
                return value.getBoolean() ? "true" : "false";
            case network::json::JsonValueKind::Number:
                return value.getNumber();
            case network::json::JsonValueKind::String:
                return value.getString();
            case network::json::JsonValueKind::Null:
            case network::json::JsonValueKind::Array:
            case network::json::JsonValueKind::Object:
                fail("Request metadata values must be scalar and non-null.");
        }
        fail("Unknown JSON request metadata value.");
    }

    // Converts a lowered request method into the transport method.
    network::HttpMethod IrInterpreter::resolveHttpMethod(
        compiler::ir::IrHttpMethod method
    ) noexcept {
        switch (method) {
            case compiler::ir::IrHttpMethod::Get:
                return network::HttpMethod::Get;
            case compiler::ir::IrHttpMethod::Post:
                return network::HttpMethod::Post;
            case compiler::ir::IrHttpMethod::Put:
                return network::HttpMethod::Put;
            case compiler::ir::IrHttpMethod::Patch:
                return network::HttpMethod::Patch;
            case compiler::ir::IrHttpMethod::Delete:
                return network::HttpMethod::Delete;
            case compiler::ir::IrHttpMethod::Head:
                return network::HttpMethod::Head;
            case compiler::ir::IrHttpMethod::Options:
                return network::HttpMethod::Options;
            case compiler::ir::IrHttpMethod::Trace:
                return network::HttpMethod::Trace;
            case compiler::ir::IrHttpMethod::Connect:
                return network::HttpMethod::Connect;
        }
        return network::HttpMethod::Get;
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
