#include "crossa/compiler/semantic/SemanticAnalyzer.h"

#include <filesystem>
#include <optional>
#include <stdexcept>
#include <string_view>
#include <utility>

using namespace std;

namespace crossa::compiler::semantic {

    // Creates a function signature from its ordered argument and result types.
    SemanticAnalyzer::FunctionSignature::FunctionSignature(
        vector<types::SemanticType> parameterTypes,
        types::SemanticType returnType
    )
        : parameterTypes_(std::move(parameterTypes)),
          returnType_(std::move(returnType)) {}

    // Returns the ordered parameter types.
    const vector<types::SemanticType>&
    SemanticAnalyzer::FunctionSignature::getParameterTypes() const noexcept {
        return parameterTypes_;
    }

    // Returns the logical result type or Unit.
    const types::SemanticType&
    SemanticAnalyzer::FunctionSignature::getReturnType() const noexcept {
        return returnType_;
    }

    // Creates an analyzer over one AST and its originating source file.
    SemanticAnalyzer::SemanticAnalyzer(
        const ast::SourceUnit& sourceUnit,
        const source::SourceFile& sourceFile
    ) noexcept
        : sourceUnit_(sourceUnit),
          sourceFile_(sourceFile),
          globalScope_(nullptr) {}

    // Validates the complete AST and returns the typed semantic source unit.
    TypedSourceUnit SemanticAnalyzer::analyze() {
        validateSourceUnitShape();
        registerTopLevelNames();
        registerFunctionSignatures();
        registerSourceVariables();

        vector<unique_ptr<TypedDeclaration>> declarations;
        declarations.reserve(sourceUnit_.getDeclarations().size());
        for (const unique_ptr<ast::Declaration>& declaration :
             sourceUnit_.getDeclarations()) {
            declarations.push_back(analyzeDeclaration(*declaration));
        }

        return TypedSourceUnit(
            sourceFile_.getPath(),
            sourceFile_.getPath().stem().string(),
            std::move(declarations)
        );
    }

    // Validates reserved config-file structure before symbol registration.
    void SemanticAnalyzer::validateSourceUnitShape() const {
        const bool isConfigFile =
            sourceFile_.getPath().filename() == filesystem::path("config.cra");
        size_t configCount = 0;

        for (const unique_ptr<ast::Declaration>& declaration :
             sourceUnit_.getDeclarations()) {
            if (declaration->getKind() == ast::DeclarationKind::Config) {
                ++configCount;
                if (!isConfigFile) {
                    fail(
                        declaration->getLocation(),
                        "CRA5001",
                        "A config declaration is only allowed in config.cra."
                    );
                }
            } else if (isConfigFile) {
                fail(
                    declaration->getLocation(),
                    "CRA5002",
                    "config.cra may contain only one config declaration."
                );
            }
        }

        if (configCount > 1) {
            fail(
                source::SourceLocation(1, 1),
                "CRA5003",
                "config.cra contains more than one config declaration."
            );
        }

        if (isConfigFile && configCount == 0) {
            fail(
                source::SourceLocation(1, 1),
                "CRA5004",
                "config.cra must contain one config declaration."
            );
        }

        if (sourceUnit_.getDeclarations().size() == 1 &&
            sourceUnit_.getDeclarations().front()->getKind() ==
                ast::DeclarationKind::Model) {
            const auto& model = static_cast<const ast::ModelDeclaration&>(
                *sourceUnit_.getDeclarations().front()
            );
            const string_view declarationSourcePath =
                model.getLocation().getSourcePath();
            const bool belongsToEntrySource = declarationSourcePath.empty() ||
                filesystem::path(string(declarationSourcePath)) ==
                    sourceFile_.getPath();
            const string sourceIdentity =
                sourceFile_.getPath().stem().string();
            if (belongsToEntrySource && model.getName() != sourceIdentity) {
                fail(
                    model.getLocation(),
                    "CRA4002",
                    "Model '" + model.getName() +
                    "' must match source identity '" + sourceIdentity + "'."
                );
            }
        }
    }

    // Registers every top-level name and model type before type resolution.
    void SemanticAnalyzer::registerTopLevelNames() {
        for (const unique_ptr<ast::Declaration>& declaration :
             sourceUnit_.getDeclarations()) {
            const string* name = getDeclarationName(*declaration);
            if (name == nullptr) {
                continue;
            }

            if (!topLevelNames_.insert(*name).second) {
                fail(
                    declaration->getLocation(),
                    "CRA2001",
                    "Duplicate top-level symbol '" + *name + "'."
                );
            }

            if (declaration->getKind() == ast::DeclarationKind::Model) {
                modelNames_.insert(*name);
            }
        }
    }

    // Resolves and registers all callable signatures before body analysis.
    void SemanticAnalyzer::registerFunctionSignatures() {
        for (const unique_ptr<ast::Declaration>& declaration :
             sourceUnit_.getDeclarations()) {
            if (declaration->getKind() != ast::DeclarationKind::Function) {
                continue;
            }

            const auto& function =
                static_cast<const ast::FunctionDeclaration&>(*declaration);
            vector<types::SemanticType> parameterTypes;
            parameterTypes.reserve(function.getParameters().size());
            unordered_set<string> parameterNames;

            for (const ast::Parameter& parameter : function.getParameters()) {
                if (!parameterNames.insert(parameter.getName()).second) {
                    fail(
                        parameter.getLocation(),
                        "CRA2002",
                        "Duplicate parameter '" + parameter.getName() +
                        "' in function '" + function.getName() + "'."
                    );
                }
                parameterTypes.push_back(resolveType(parameter.getType()));
            }

            const ast::TypeReference* astReturnType = function.getReturnType();
            types::SemanticType returnType = astReturnType == nullptr
                ? types::SemanticType::createUnit()
                : resolveType(*astReturnType);
            functionSignatures_.emplace(
                function.getName(),
                FunctionSignature(
                    std::move(parameterTypes),
                    std::move(returnType)
                )
            );
        }
    }

    // Resolves and registers all source variables in the global value scope.
    void SemanticAnalyzer::registerSourceVariables() {
        for (const unique_ptr<ast::Declaration>& declaration :
             sourceUnit_.getDeclarations()) {
            if (declaration->getKind() != ast::DeclarationKind::Variable) {
                continue;
            }

            const auto& variable =
                static_cast<const ast::VariableDeclaration&>(*declaration);
            const bool declared = globalScope_.declare(
                variable.getName(),
                ValueSymbol(
                    resolveType(variable.getType()),
                    ValueSymbolKind::SourceVariable
                )
            );
            if (!declared) {
                fail(
                    variable.getLocation(),
                    "CRA2003",
                    "Duplicate source variable '" + variable.getName() + "'."
                );
            }
        }
    }

    // Converts one AST declaration into a validated typed declaration.
    unique_ptr<TypedDeclaration> SemanticAnalyzer::analyzeDeclaration(
        const ast::Declaration& declaration
    ) {
        switch (declaration.getKind()) {
            case ast::DeclarationKind::Import:
                fail(
                    declaration.getLocation(),
                    "CRA1001",
                    "Unresolved import reached semantic analysis."
                );
            case ast::DeclarationKind::Variable:
                return analyzeVariableDeclaration(
                    static_cast<const ast::VariableDeclaration&>(declaration)
                );
            case ast::DeclarationKind::Model:
                return analyzeModelDeclaration(
                    static_cast<const ast::ModelDeclaration&>(declaration)
                );
            case ast::DeclarationKind::Function:
                return analyzeFunctionDeclaration(
                    static_cast<const ast::FunctionDeclaration&>(declaration)
                );
            case ast::DeclarationKind::Config:
                return analyzeConfigDeclaration(
                    static_cast<const ast::ConfigDeclaration&>(declaration)
                );
            case ast::DeclarationKind::Expression:
                return analyzeExpressionDeclaration(
                    static_cast<const ast::ExpressionDeclaration&>(declaration)
                );
        }

        fail(
            declaration.getLocation(),
            "CRA9001",
            "Unknown AST declaration kind."
        );
    }

    // Validates and converts one source variable declaration.
    unique_ptr<TypedDeclaration>
    SemanticAnalyzer::analyzeVariableDeclaration(
        const ast::VariableDeclaration& declaration
    ) {
        const ValueSymbol* symbol = globalScope_.resolve(declaration.getName());
        if (symbol == nullptr) {
            fail(
                declaration.getLocation(),
                "CRA9002",
                "Source variable was not registered."
            );
        }

        unique_ptr<TypedExpression> initializer = analyzeExpression(
            declaration.getInitializer(),
            globalScope_,
            &symbol->getType()
        );
        if (initializer->getType() != symbol->getType()) {
            fail(
                declaration.getInitializer().getLocation(),
                "CRA3001",
                "Variable '" + declaration.getName() + "' expects '" +
                symbol->getType().format() + "' but received '" +
                initializer->getType().format() + "'."
            );
        }

        return make_unique<TypedVariableDeclaration>(
            declaration.getName(),
            symbol->getType(),
            std::move(initializer),
            declaration.getLocation()
        );
    }

    // Validates and converts one model declaration.
    unique_ptr<TypedDeclaration> SemanticAnalyzer::analyzeModelDeclaration(
        const ast::ModelDeclaration& declaration
    ) {
        vector<TypedModelField> fields;
        fields.reserve(declaration.getFields().size());
        unordered_set<string> fieldNames;

        for (const ast::ModelField& field : declaration.getFields()) {
            if (!fieldNames.insert(field.getName()).second) {
                fail(
                    field.getLocation(),
                    "CRA4001",
                    "Duplicate field '" + field.getName() + "' in model '" +
                    declaration.getName() + "'."
                );
            }
            fields.emplace_back(
                field.getName(),
                resolveType(field.getType()),
                field.getLocation()
            );
        }

        return make_unique<TypedModelDeclaration>(
            declaration.getName(),
            std::move(fields),
            declaration.getLocation()
        );
    }

    // Validates and converts one function declaration and its lexical scope.
    unique_ptr<TypedDeclaration>
    SemanticAnalyzer::analyzeFunctionDeclaration(
        const ast::FunctionDeclaration& declaration
    ) {
        const auto signatureIterator =
            functionSignatures_.find(declaration.getName());
        if (signatureIterator == functionSignatures_.end()) {
            fail(
                declaration.getLocation(),
                "CRA9003",
                "Function signature was not registered."
            );
        }

        const FunctionSignature& signature = signatureIterator->second;
        const SemanticExecutionPolicy executionPolicy =
            resolveExecutionPolicy(declaration.getExecutionPolicy());
        const types::SemanticType& returnType = signature.getReturnType();

        if (executionPolicy == SemanticExecutionPolicy::Async &&
            returnType.getKind() != types::SemanticTypeKind::Unit) {
            fail(
                declaration.getLocation(),
                "CRA6001",
                "@Async is fire-and-forget and cannot declare a result type."
            );
        }
        if (executionPolicy == SemanticExecutionPolicy::AsyncAfter &&
            returnType.getKind() == types::SemanticTypeKind::Unit) {
            fail(
                declaration.getLocation(),
                "CRA6002",
                "@AsyncAfter requires a logical result type."
            );
        }

        SemanticScope functionScope(&globalScope_);
        vector<TypedParameter> parameters;
        parameters.reserve(declaration.getParameters().size());
        for (size_t index = 0;
             index < declaration.getParameters().size();
             ++index) {
            const ast::Parameter& parameter = declaration.getParameters()[index];
            const types::SemanticType& parameterType =
                signature.getParameterTypes()[index];
            const bool declared = functionScope.declare(
                parameter.getName(),
                ValueSymbol(parameterType, ValueSymbolKind::Parameter)
            );
            if (!declared) {
                fail(
                    parameter.getLocation(),
                    "CRA2002",
                    "Duplicate parameter '" + parameter.getName() + "'."
                );
            }
            parameters.emplace_back(
                parameter.getName(),
                parameterType,
                parameter.getLocation()
            );
        }

        vector<unique_ptr<TypedStatement>> statements;
        statements.reserve(declaration.getStatements().size());
        bool hasReturn = false;
        for (const unique_ptr<ast::Statement>& statement :
             declaration.getStatements()) {
            statements.push_back(
                analyzeStatement(
                    *statement,
                    functionScope,
                    returnType,
                    hasReturn
                )
            );
        }

        if (returnType.getKind() != types::SemanticTypeKind::Unit &&
            !hasReturn) {
            fail(
                declaration.getLocation(),
                "CRA3002",
                "Function '" + declaration.getName() + "' must return '" +
                returnType.format() + "'."
            );
        }

        return make_unique<TypedFunctionDeclaration>(
            declaration.getName(),
            executionPolicy,
            std::move(parameters),
            returnType,
            std::move(statements),
            declaration.getLocation()
        );
    }

    // Validates and converts one config declaration.
    unique_ptr<TypedDeclaration> SemanticAnalyzer::analyzeConfigDeclaration(
        const ast::ConfigDeclaration& declaration
    ) {
        vector<TypedConfigEntry> entries;
        entries.reserve(declaration.getEntries().size());
        unordered_set<string> entryNames;

        for (const ast::ConfigEntry& entry : declaration.getEntries()) {
            if (!entryNames.insert(entry.getName()).second) {
                fail(
                    entry.getLocation(),
                    "CRA5005",
                    "Duplicate config key '" + entry.getName() + "'."
                );
            }

            optional<types::SemanticType> expectedType =
                getConfigType(entry.getName());
            if (!expectedType.has_value()) {
                fail(
                    entry.getLocation(),
                    "CRA5006",
                    "Unknown config key '" + entry.getName() + "'."
                );
            }

            unique_ptr<TypedExpression> value = analyzeExpression(
                entry.getValue(),
                globalScope_
            );
            const bool compatibleInterceptor =
                entry.getName() == "interceptor" &&
                value->getType().getKind() == types::SemanticTypeKind::Bool;
            if (value->getType() != *expectedType && !compatibleInterceptor) {
                fail(
                    entry.getValue().getLocation(),
                    "CRA5007",
                    "Config key '" + entry.getName() + "' expects '" +
                    expectedType->format() + "' but received '" +
                    value->getType().format() + "'."
                );
            }
            if (entry.getName() == "commonHeaders") {
                validateRequestMap(*value, "commonHeaders");
            }
            if (entry.getName() == "interceptor" &&
                value->getType().getKind() ==
                    types::SemanticTypeKind::Json &&
                value->getKind() != TypedExpressionKind::JsonObject) {
                fail(
                    entry.getLocation(),
                    "CRA5008",
                    "Config interceptor expects Bool or a JSON object."
                );
            }
            if (entry.getName() == "interceptor" &&
                value->getKind() == TypedExpressionKind::JsonObject) {
                const auto& interceptor = static_cast<
                    const TypedJsonObjectExpression&
                >(*value);
                const unordered_set<string> supportedOptions{
                    "enabled",
                    "logRequests",
                    "logResponses",
                    "logHeaders",
                    "logBody",
                    "excludedHeaders"
                };
                for (const TypedJsonObjectEntry& option :
                     interceptor.getEntries()) {
                    if (!supportedOptions.contains(option.getKey())) {
                        fail(
                            option.getLocation(),
                            "CRA5009",
                            "Unknown interceptor option '" +
                            option.getKey() + "'."
                        );
                    }
                    if (option.getKey() == "excludedHeaders") {
                        if (option.getValue().getKind() !=
                            TypedExpressionKind::JsonArray) {
                            fail(
                                option.getLocation(),
                                "CRA5011",
                                "Interceptor excludedHeaders expects an array."
                            );
                        }
                        const auto& excludedHeaders = static_cast<
                            const TypedJsonArrayExpression&
                        >(option.getValue());
                        for (const unique_ptr<TypedExpression>& header :
                             excludedHeaders.getValues()) {
                            if (header->getType().getKind() !=
                                types::SemanticTypeKind::String) {
                                fail(
                                    header->getLocation(),
                                    "CRA5012",
                                    "Interceptor excludedHeaders values "
                                    "must be String."
                                );
                            }
                        }
                        continue;
                    }
                    if (option.getValue().getType().getKind() !=
                        types::SemanticTypeKind::Bool) {
                        fail(
                            option.getLocation(),
                            "CRA5010",
                            "Interceptor option '" + option.getKey() +
                            "' expects Bool."
                        );
                    }
                }
            }

            const unordered_set<string> objectConfigKeys{
                "retryPolicy",
                "authProviders",
                "proxy",
                "certificatePolicy",
                "telemetry"
            };
            if (objectConfigKeys.contains(entry.getName()) &&
                value->getKind() != TypedExpressionKind::JsonObject) {
                fail(
                    entry.getLocation(),
                    "CRA5013",
                    "Config key '" + entry.getName() +
                    "' expects a JSON object."
                );
            }

            entries.emplace_back(
                entry.getName(),
                *expectedType,
                std::move(value),
                entry.getLocation()
            );
        }

        return make_unique<TypedConfigDeclaration>(
            std::move(entries),
            declaration.getLocation()
        );
    }

    // Validates and converts one top-level executable expression.
    unique_ptr<TypedDeclaration>
    SemanticAnalyzer::analyzeExpressionDeclaration(
        const ast::ExpressionDeclaration& declaration
    ) {
        if (declaration.getExpression().getKind() !=
            ast::ExpressionKind::Call) {
            fail(
                declaration.getLocation(),
                "CRA2008",
                "A top-level executable expression must be a function call."
            );
        }

        return make_unique<TypedExpressionDeclaration>(
            analyzeExpression(declaration.getExpression(), globalScope_),
            declaration.getLocation()
        );
    }

    // Validates and converts one function-body statement.
    unique_ptr<TypedStatement> SemanticAnalyzer::analyzeStatement(
        const ast::Statement& statement,
        SemanticScope& scope,
        const types::SemanticType& returnType,
        bool& hasReturn
    ) {
        switch (statement.getKind()) {
            case ast::StatementKind::Return: {
                const auto& returnStatement =
                    static_cast<const ast::ReturnStatement&>(statement);
                if (returnType.getKind() == types::SemanticTypeKind::Unit) {
                    fail(
                        statement.getLocation(),
                        "CRA3003",
                        "A no-value function cannot return a value."
                    );
                }

                unique_ptr<TypedExpression> expression =
                    returnStatement.getExpression().getKind() ==
                            ast::ExpressionKind::CrossaRequest
                        ? analyzeCrossaRequestExpression(
                              static_cast<
                                  const ast::CrossaRequestExpression&
                              >(returnStatement.getExpression()),
                              scope,
                              returnType
                          )
                        : analyzeExpression(
                              returnStatement.getExpression(),
                              scope,
                              &returnType
                          );
                if (expression->getType() != returnType) {
                    fail(
                        returnStatement.getExpression().getLocation(),
                        "CRA3004",
                        "Return expects '" + returnType.format() +
                        "' but received '" +
                        expression->getType().format() + "'."
                    );
                }
                hasReturn = true;
                return make_unique<TypedReturnStatement>(
                    std::move(expression),
                    statement.getLocation()
                );
            }
            case ast::StatementKind::Expression: {
                const auto& expressionStatement =
                    static_cast<const ast::ExpressionStatement&>(statement);
                if (expressionStatement.getExpression().getKind() ==
                    ast::ExpressionKind::CrossaRequest) {
                    return make_unique<TypedExpressionStatement>(
                        analyzeCrossaRequestExpression(
                            static_cast<
                                const ast::CrossaRequestExpression&
                            >(expressionStatement.getExpression()),
                            scope,
                            types::SemanticType::createUnit()
                        ),
                        statement.getLocation()
                    );
                }
                return make_unique<TypedExpressionStatement>(
                    analyzeExpression(expressionStatement.getExpression(), scope),
                    statement.getLocation()
                );
            }
            case ast::StatementKind::Variable:
                return analyzeVariableStatement(
                    static_cast<const ast::VariableStatement&>(statement),
                    scope
                );
            case ast::StatementKind::If:
                return analyzeIfStatement(
                    static_cast<const ast::IfStatement&>(statement),
                    scope,
                    returnType,
                    hasReturn
                );
        }

        fail(
            statement.getLocation(),
            "CRA9004",
            "Unknown AST statement kind."
        );
    }

    unique_ptr<TypedStatement> SemanticAnalyzer::analyzeIfStatement(
        const ast::IfStatement& statement,
        SemanticScope& scope,
        const types::SemanticType& returnType,
        bool& hasReturn
    ) {
        unique_ptr<TypedExpression> condition = analyzeExpression(
            statement.getCondition(),
            scope
        );
        if (condition->getType().getKind() != types::SemanticTypeKind::Bool) {
            fail(
                statement.getCondition().getLocation(),
                "CRA3013",
                "An if condition must be Bool but received '" +
                condition->getType().format() + "'."
            );
        }

        vector<unique_ptr<TypedStatement>> thenStatements;
        SemanticScope thenScope(&scope);
        bool thenReturns = false;
        for (const unique_ptr<ast::Statement>& nestedStatement :
             statement.getThenStatements()) {
            thenStatements.push_back(analyzeStatement(
                *nestedStatement,
                thenScope,
                returnType,
                thenReturns
            ));
        }

        optional<vector<unique_ptr<TypedStatement>>> elseStatements;
        bool elseReturns = false;
        if (const vector<unique_ptr<ast::Statement>>* branch =
                statement.getElseStatements();
            branch != nullptr) {
            vector<unique_ptr<TypedStatement>> typedElseStatements;
            SemanticScope elseScope(&scope);
            for (const unique_ptr<ast::Statement>& nestedStatement : *branch) {
                typedElseStatements.push_back(analyzeStatement(
                    *nestedStatement,
                    elseScope,
                    returnType,
                    elseReturns
                ));
            }
            elseStatements.emplace(std::move(typedElseStatements));
        }

        hasReturn = thenReturns && elseReturns &&
            statement.getElseStatements() != nullptr;
        return make_unique<TypedIfStatement>(
            std::move(condition),
            std::move(thenStatements),
            std::move(elseStatements),
            statement.getLocation()
        );
    }

    // Validates and converts one local variable statement.
    unique_ptr<TypedStatement> SemanticAnalyzer::analyzeVariableStatement(
        const ast::VariableStatement& statement,
        SemanticScope& scope
    ) {
        types::SemanticType type = resolveType(statement.getType());
        unique_ptr<TypedExpression> initializer = analyzeExpression(
            statement.getInitializer(),
            scope,
            &type
        );
        if (initializer->getType() != type) {
            fail(
                statement.getInitializer().getLocation(),
                "CRA3005",
                "Local variable '" + statement.getName() + "' expects '" +
                type.format() + "' but received '" +
                initializer->getType().format() + "'."
            );
        }

        const bool declared = scope.declare(
            statement.getName(),
            ValueSymbol(type, ValueSymbolKind::LocalVariable)
        );
        if (!declared) {
            fail(
                statement.getLocation(),
                "CRA2004",
                "Duplicate local symbol '" + statement.getName() + "'."
            );
        }

        return make_unique<TypedVariableStatement>(
            statement.getName(),
            std::move(type),
            std::move(initializer),
            statement.getLocation()
        );
    }

    // Resolves and converts one AST expression in the provided lexical scope.
    unique_ptr<TypedExpression> SemanticAnalyzer::analyzeExpression(
        const ast::Expression& expression,
        const SemanticScope& scope
    ) {
        return analyzeExpression(expression, scope, nullptr);
    }

    // Resolves one expression with an optional contextual expected type.
    unique_ptr<TypedExpression> SemanticAnalyzer::analyzeExpression(
        const ast::Expression& expression,
        const SemanticScope& scope,
        const types::SemanticType* expectedType
    ) {
        switch (expression.getKind()) {
            case ast::ExpressionKind::Identifier:
                return analyzeIdentifierExpression(
                    static_cast<const ast::IdentifierExpression&>(expression),
                    scope
                );
            case ast::ExpressionKind::IntegerLiteral: {
                const auto& literal =
                    static_cast<const ast::IntegerLiteralExpression&>(expression);
                if (expectedType != nullptr &&
                    expectedType->getKind() ==
                        types::SemanticTypeKind::Double) {
                    return make_unique<TypedDecimalLiteralExpression>(
                        literal.getValue(),
                        expression.getLocation()
                    );
                }
                return make_unique<TypedIntegerLiteralExpression>(
                    literal.getValue(),
                    expectedType != nullptr &&
                            expectedType->getKind() ==
                                types::SemanticTypeKind::Long
                        ? types::SemanticType::createLong()
                        : types::SemanticType::createInt(),
                    expression.getLocation()
                );
            }
            case ast::ExpressionKind::DecimalLiteral: {
                const auto& literal =
                    static_cast<const ast::DecimalLiteralExpression&>(
                        expression
                    );
                return make_unique<TypedDecimalLiteralExpression>(
                    literal.getValue(),
                    expression.getLocation()
                );
            }
            case ast::ExpressionKind::StringLiteral:
                return analyzeStringExpression(
                    static_cast<const ast::StringLiteralExpression&>(expression),
                    scope
                );
            case ast::ExpressionKind::BooleanLiteral: {
                const auto& literal =
                    static_cast<const ast::BooleanLiteralExpression&>(expression);
                return make_unique<TypedBooleanLiteralExpression>(
                    literal.getValue(),
                    expression.getLocation()
                );
            }
            case ast::ExpressionKind::Call:
                return analyzeCallExpression(
                    static_cast<const ast::CallExpression&>(expression),
                    scope
                );
            case ast::ExpressionKind::Unary:
                return analyzeUnaryExpression(
                    static_cast<const ast::UnaryExpression&>(expression),
                    scope,
                    expectedType
                );
            case ast::ExpressionKind::Binary:
                return analyzeBinaryExpression(
                    static_cast<const ast::BinaryExpression&>(expression),
                    scope,
                    expectedType
                );
            case ast::ExpressionKind::JsonNumber: {
                const auto& number =
                    static_cast<const ast::JsonNumberExpression&>(expression);
                return make_unique<TypedJsonNumberExpression>(
                    number.getValue(),
                    number.getLocation()
                );
            }
            case ast::ExpressionKind::JsonNull:
                return make_unique<TypedJsonNullExpression>(
                    expression.getLocation()
                );
            case ast::ExpressionKind::JsonObject:
                return analyzeJsonObjectExpression(
                    static_cast<const ast::JsonObjectExpression&>(expression),
                    scope
                );
            case ast::ExpressionKind::JsonArray:
                return analyzeJsonArrayExpression(
                    static_cast<const ast::JsonArrayExpression&>(expression),
                    scope
                );
            case ast::ExpressionKind::HttpMethod:
                fail(
                    expression.getLocation(),
                    "CRA7004",
                    "An HTTP method literal is valid only in CrossaRequest."
                );
            case ast::ExpressionKind::CrossaRequest:
                fail(
                    expression.getLocation(),
                    "CRA7001",
                    "CrossaRequest must be a direct function return or statement."
                );
        }

        fail(
            expression.getLocation(),
            "CRA9005",
            "Unknown AST expression kind."
        );
    }

    // Resolves one identifier expression to a visible value symbol.
    unique_ptr<TypedExpression>
    SemanticAnalyzer::analyzeIdentifierExpression(
        const ast::IdentifierExpression& expression,
        const SemanticScope& scope
    ) {
        const ValueSymbol* symbol = scope.resolve(expression.getName());
        if (symbol == nullptr) {
            fail(
                expression.getLocation(),
                "CRA2005",
                "Unknown value symbol '" + expression.getName() + "'."
            );
        }

        return make_unique<TypedIdentifierExpression>(
            expression.getName(),
            symbol->getKind(),
            symbol->getType(),
            expression.getLocation()
        );
    }

    // Resolves every interpolation segment in one string expression.
    unique_ptr<TypedExpression> SemanticAnalyzer::analyzeStringExpression(
        const ast::StringLiteralExpression& expression,
        const SemanticScope& scope
    ) {
        vector<TypedStringSegment> segments;
        segments.reserve(expression.getSegments().size());

        for (const ast::StringSegment& segment : expression.getSegments()) {
            if (segment.getKind() == ast::StringSegmentKind::Literal) {
                segments.emplace_back(
                    TypedStringSegmentKind::Literal,
                    segment.getValue(),
                    types::SemanticType::createString(),
                    nullopt,
                    segment.getLocation()
                );
                continue;
            }

            const ValueSymbol* symbol = scope.resolve(segment.getValue());
            if (symbol == nullptr) {
                fail(
                    segment.getLocation(),
                    "CRA2006",
                    "Unknown interpolation identifier '" +
                    segment.getValue() + "'."
                );
            }
            if (symbol->getType().getKind() == types::SemanticTypeKind::Unit) {
                fail(
                    segment.getLocation(),
                    "CRA3006",
                    "Interpolation cannot render a Unit value."
                );
            }

            segments.emplace_back(
                TypedStringSegmentKind::Symbol,
                segment.getValue(),
                symbol->getType(),
                symbol->getKind(),
                segment.getLocation()
            );
        }

        return make_unique<TypedStringLiteralExpression>(
            std::move(segments),
            expression.getLocation()
        );
    }

    // Validates one function or language builtin call.
    unique_ptr<TypedExpression> SemanticAnalyzer::analyzeCallExpression(
        const ast::CallExpression& expression,
        const SemanticScope& scope
    ) {
        if (expression.getCallee() == "print") {
            vector<unique_ptr<TypedExpression>> arguments;
            arguments.reserve(expression.getArguments().size());
            for (const unique_ptr<ast::Expression>& argument :
                 expression.getArguments()) {
                arguments.push_back(analyzeExpression(*argument, scope));
            }
            if (arguments.size() != 1) {
                fail(
                    expression.getLocation(),
                    "CRA3007",
                    "print expects exactly one argument."
                );
            }
            if (arguments.front()->getType().getKind() ==
                types::SemanticTypeKind::Unit) {
                fail(
                    arguments.front()->getLocation(),
                    "CRA3008",
                    "print cannot render a Unit value."
                );
            }

            return make_unique<TypedCallExpression>(
                expression.getCallee(),
                true,
                std::move(arguments),
                types::SemanticType::createUnit(),
                expression.getLocation()
            );
        }

        if (expression.getCallee() == "assert") {
            vector<unique_ptr<TypedExpression>> arguments;
            arguments.reserve(expression.getArguments().size());
            for (const unique_ptr<ast::Expression>& argument :
                 expression.getArguments()) {
                arguments.push_back(analyzeExpression(*argument, scope));
            }
            if (arguments.empty() || arguments.size() > 2) {
                fail(
                    expression.getLocation(),
                    "CRA3014",
                    "assert expects one Bool and an optional String message."
                );
            }
            if (arguments.front()->getType().getKind() !=
                types::SemanticTypeKind::Bool) {
                fail(
                    arguments.front()->getLocation(),
                    "CRA3015",
                    "assert expects a Bool condition."
                );
            }
            if (arguments.size() == 2 &&
                arguments[1]->getType().getKind() !=
                    types::SemanticTypeKind::String) {
                fail(
                    arguments[1]->getLocation(),
                    "CRA3016",
                    "assert message expects a String."
                );
            }

            return make_unique<TypedCallExpression>(
                expression.getCallee(),
                true,
                std::move(arguments),
                types::SemanticType::createUnit(),
                expression.getLocation()
            );
        }

        const auto signatureIterator =
            functionSignatures_.find(expression.getCallee());
        if (signatureIterator == functionSignatures_.end()) {
            fail(
                expression.getLocation(),
                "CRA2007",
                "Unknown function '" + expression.getCallee() + "'."
            );
        }

        const FunctionSignature& signature = signatureIterator->second;
        if (expression.getArguments().size() !=
            signature.getParameterTypes().size()) {
            fail(
                expression.getLocation(),
                "CRA3009",
                "Function '" + expression.getCallee() + "' expects " +
                to_string(signature.getParameterTypes().size()) +
                " arguments but received " +
                to_string(expression.getArguments().size()) + "."
            );
        }

        vector<unique_ptr<TypedExpression>> arguments;
        arguments.reserve(expression.getArguments().size());
        for (size_t index = 0; index < expression.getArguments().size();
             ++index) {
            arguments.push_back(analyzeExpression(
                *expression.getArguments()[index],
                scope,
                &signature.getParameterTypes()[index]
            ));
        }

        for (size_t index = 0; index < arguments.size(); ++index) {
            const types::SemanticType& expectedType =
                signature.getParameterTypes()[index];
            if (arguments[index]->getType() != expectedType) {
                fail(
                    arguments[index]->getLocation(),
                    "CRA3010",
                    "Argument " + to_string(index + 1) + " of function '" +
                    expression.getCallee() + "' expects '" +
                    expectedType.format() + "' but received '" +
                    arguments[index]->getType().format() + "'."
                );
            }
        }

        return make_unique<TypedCallExpression>(
            expression.getCallee(),
            false,
            std::move(arguments),
            signature.getReturnType(),
            expression.getLocation()
        );
    }

    // Validates one unary arithmetic expression.
    unique_ptr<TypedExpression> SemanticAnalyzer::analyzeUnaryExpression(
        const ast::UnaryExpression& expression,
        const SemanticScope& scope,
        const types::SemanticType* expectedType
    ) {
        unique_ptr<TypedExpression> operand = analyzeExpression(
            expression.getOperand(),
            scope,
            expectedType
        );
        if (expression.getOperator() == ast::UnaryOperator::Not) {
            if (operand->getType().getKind() != types::SemanticTypeKind::Bool) {
                fail(
                    operand->getLocation(),
                    "CRA3017",
                    "Unary '!' requires a Bool operand but received '" +
                    operand->getType().format() + "'."
                );
            }
            return make_unique<TypedUnaryExpression>(
                TypedUnaryOperator::Not,
                std::move(operand),
                types::SemanticType::createBool(),
                expression.getLocation()
            );
        }
        if (!isNumericType(operand->getType())) {
            fail(
                operand->getLocation(),
                "CRA3011",
                "Unary '-' requires a numeric operand but received '" +
                operand->getType().format() + "'."
            );
        }

        types::SemanticType resultType = operand->getType();
        return make_unique<TypedUnaryExpression>(
            TypedUnaryOperator::Negate,
            std::move(operand),
            std::move(resultType),
            expression.getLocation()
        );
    }

    // Validates one binary arithmetic expression.
    unique_ptr<TypedExpression> SemanticAnalyzer::analyzeBinaryExpression(
        const ast::BinaryExpression& expression,
        const SemanticScope& scope,
        const types::SemanticType* expectedType
    ) {
        unique_ptr<TypedExpression> left = analyzeExpression(
            expression.getLeft(),
            scope,
            expectedType
        );
        unique_ptr<TypedExpression> right = analyzeExpression(
            expression.getRight(),
            scope,
            expectedType
        );
        TypedBinaryOperator operation = TypedBinaryOperator::Add;
        switch (expression.getOperator()) {
            case ast::BinaryOperator::Add:
                operation = TypedBinaryOperator::Add;
                break;
            case ast::BinaryOperator::Subtract:
                operation = TypedBinaryOperator::Subtract;
                break;
            case ast::BinaryOperator::Multiply:
                operation = TypedBinaryOperator::Multiply;
                break;
            case ast::BinaryOperator::Divide:
                operation = TypedBinaryOperator::Divide;
                break;
            case ast::BinaryOperator::Equal:
                operation = TypedBinaryOperator::Equal;
                break;
            case ast::BinaryOperator::NotEqual:
                operation = TypedBinaryOperator::NotEqual;
                break;
            case ast::BinaryOperator::Less:
                operation = TypedBinaryOperator::Less;
                break;
            case ast::BinaryOperator::LessEqual:
                operation = TypedBinaryOperator::LessEqual;
                break;
            case ast::BinaryOperator::Greater:
                operation = TypedBinaryOperator::Greater;
                break;
            case ast::BinaryOperator::GreaterEqual:
                operation = TypedBinaryOperator::GreaterEqual;
                break;
            case ast::BinaryOperator::LogicalAnd:
                operation = TypedBinaryOperator::LogicalAnd;
                break;
            case ast::BinaryOperator::LogicalOr:
                operation = TypedBinaryOperator::LogicalOr;
                break;
        }

        const bool equality = expression.getOperator() == ast::BinaryOperator::Equal ||
            expression.getOperator() == ast::BinaryOperator::NotEqual;
        const bool comparison = equality ||
            expression.getOperator() == ast::BinaryOperator::Less ||
            expression.getOperator() == ast::BinaryOperator::LessEqual ||
            expression.getOperator() == ast::BinaryOperator::Greater ||
            expression.getOperator() == ast::BinaryOperator::GreaterEqual;
        const bool logical = expression.getOperator() == ast::BinaryOperator::LogicalAnd ||
            expression.getOperator() == ast::BinaryOperator::LogicalOr;
        if (logical) {
            if (left->getType().getKind() != types::SemanticTypeKind::Bool ||
                right->getType().getKind() != types::SemanticTypeKind::Bool) {
                fail(
                    expression.getLocation(),
                    "CRA3018",
                    "Logical operators require Bool operands but received '" +
                    left->getType().format() + "' and '" +
                    right->getType().format() + "'."
                );
            }
            return make_unique<TypedBinaryExpression>(
                std::move(left),
                operation,
                std::move(right),
                types::SemanticType::createBool(),
                expression.getLocation()
            );
        }
        if (comparison) {
            const bool sameType = left->getType() == right->getType();
            const bool scalarEquality = equality &&
                (left->getType().getKind() == types::SemanticTypeKind::Bool ||
                 left->getType().getKind() == types::SemanticTypeKind::String ||
                 isNumericType(left->getType()));
            const bool numericComparison = !equality &&
                isNumericType(left->getType());
            if (!sameType || (!scalarEquality && !numericComparison)) {
                fail(
                    expression.getLocation(),
                    "CRA3014",
                    "Comparison operands are incompatible: '" +
                    left->getType().format() + "' and '" +
                    right->getType().format() + "'."
                );
            }
            return make_unique<TypedBinaryExpression>(
                std::move(left),
                operation,
                std::move(right),
                types::SemanticType::createBool(),
                expression.getLocation()
            );
        }

        if (!isNumericType(left->getType()) || !isNumericType(right->getType())) {
            fail(
                expression.getLocation(),
                "CRA3012",
                "Arithmetic operators require numeric operands but received '" +
                left->getType().format() + "' and '" +
                right->getType().format() + "'."
            );
        }
        if (left->getType() != right->getType()) {
            fail(
                expression.getLocation(),
                "CRA3012",
                "Arithmetic operands must have the same numeric type but "
                "received '" + left->getType().format() + "' and '" +
                right->getType().format() + "'."
            );
        }

        types::SemanticType resultType = left->getType();
        return make_unique<TypedBinaryExpression>(
            std::move(left),
            operation,
            std::move(right),
            std::move(resultType),
            expression.getLocation()
        );
    }

    // Validates one JSON object and recursively types every field value.
    unique_ptr<TypedExpression>
    SemanticAnalyzer::analyzeJsonObjectExpression(
        const ast::JsonObjectExpression& expression,
        const SemanticScope& scope
    ) {
        vector<TypedJsonObjectEntry> entries;
        entries.reserve(expression.getEntries().size());
        unordered_set<string> keys;
        for (const ast::JsonObjectEntry& entry : expression.getEntries()) {
            if (!keys.insert(entry.getKey()).second) {
                fail(
                    entry.getLocation(),
                    "CRA7010",
                    "Duplicate JSON object key '" + entry.getKey() + "'."
                );
            }
            unique_ptr<TypedExpression> value = analyzeExpression(
                entry.getValue(),
                scope
            );
            if (value->getType().getKind() ==
                    types::SemanticTypeKind::Unit ||
                value->getType().getKind() ==
                    types::SemanticTypeKind::Model ||
                value->getType().getKind() ==
                    types::SemanticTypeKind::List) {
                fail(
                    entry.getLocation(),
                    "CRA7011",
                    "JSON field '" + entry.getKey() +
                    "' requires a JSON-compatible scalar or Json value."
                );
            }
            entries.emplace_back(
                entry.getKey(),
                std::move(value),
                entry.getLocation()
            );
        }
        return make_unique<TypedJsonObjectExpression>(
            std::move(entries),
            expression.getLocation()
        );
    }

    // Validates one JSON array and recursively types every item.
    unique_ptr<TypedExpression>
    SemanticAnalyzer::analyzeJsonArrayExpression(
        const ast::JsonArrayExpression& expression,
        const SemanticScope& scope
    ) {
        vector<unique_ptr<TypedExpression>> values;
        values.reserve(expression.getValues().size());
        for (const unique_ptr<ast::Expression>& item : expression.getValues()) {
            unique_ptr<TypedExpression> value = analyzeExpression(*item, scope);
            if (value->getType().getKind() ==
                    types::SemanticTypeKind::Unit ||
                value->getType().getKind() ==
                    types::SemanticTypeKind::Model ||
                value->getType().getKind() ==
                    types::SemanticTypeKind::List) {
                fail(
                    item->getLocation(),
                    "CRA7012",
                    "JSON arrays require JSON-compatible scalar or Json values."
                );
            }
            values.push_back(std::move(value));
        }
        return make_unique<TypedJsonArrayExpression>(
            std::move(values),
            expression.getLocation()
        );
    }

    // Validates a direct request expression against its function result type.
    unique_ptr<TypedExpression>
    SemanticAnalyzer::analyzeCrossaRequestExpression(
        const ast::CrossaRequestExpression& expression,
        const SemanticScope& scope,
        const types::SemanticType& responseType
    ) {
        const unordered_set<string> supportedFields{
            "url",
            "path",
            "method",
            "headers",
            "customHeaders",
            "queryParams",
            "pathVariables",
            "body",
            "timeout",
            "retryPolicy",
            "auth",
            "multipart",
            "uploadProgress",
            "downloadStreaming",
            "coalesce",
            "proxy",
            "certificatePolicy",
            "telemetry"
        };
        unordered_set<string> names;
        for (const ast::CrossaRequestEntry& entry : expression.getEntries()) {
            if (!supportedFields.contains(entry.getName())) {
                fail(
                    entry.getLocation(),
                    "CRA7002",
                    "Unknown CrossaRequest field '" + entry.getName() + "'."
                );
            }
            if (!names.insert(entry.getName()).second) {
                fail(
                    entry.getLocation(),
                    "CRA7003",
                    "Duplicate CrossaRequest field '" + entry.getName() + "'."
                );
            }
        }

        const ast::CrossaRequestEntry* urlEntry =
            findRequestEntry(expression, "url");
        const ast::CrossaRequestEntry* pathEntry =
            findRequestEntry(expression, "path");
        if (urlEntry != nullptr && pathEntry != nullptr) {
            fail(
                pathEntry->getLocation(),
                "CRA7005",
                "CrossaRequest accepts either 'url' or 'path', not both."
            );
        }
        if (urlEntry == nullptr) {
            urlEntry = pathEntry;
        }
        if (urlEntry == nullptr) {
            fail(
                expression.getLocation(),
                "CRA7005",
                "CrossaRequest requires a 'url' field."
            );
        }

        const ast::CrossaRequestEntry* methodEntry =
            findRequestEntry(expression, "method");
        if (methodEntry == nullptr || methodEntry->getValue().getKind() !=
            ast::ExpressionKind::HttpMethod) {
            fail(
                methodEntry == nullptr
                    ? expression.getLocation()
                    : methodEntry->getLocation(),
                "CRA7004",
                "CrossaRequest requires a supported HTTP method literal."
            );
        }
        const auto& methodExpression =
            static_cast<const ast::HttpMethodLiteralExpression&>(
                methodEntry->getValue()
            );

        const ast::CrossaRequestEntry* pathVariablesEntry =
            findRequestEntry(expression, "pathVariables");
        const ast::JsonObjectExpression* pathVariables = nullptr;
        if (pathVariablesEntry != nullptr) {
            if (pathVariablesEntry->getValue().getKind() !=
                ast::ExpressionKind::JsonObject) {
                fail(
                    pathVariablesEntry->getLocation(),
                    "CRA7006",
                    "CrossaRequest pathVariables expects a JSON object."
                );
            }
            pathVariables = &static_cast<const ast::JsonObjectExpression&>(
                pathVariablesEntry->getValue()
            );
            unique_ptr<TypedExpression> validatedPathVariables =
                analyzeExpression(pathVariablesEntry->getValue(), scope);
            validateRequestMap(*validatedPathVariables, "pathVariables");
        }

        unique_ptr<TypedExpression> url = analyzeRequestUrl(
            urlEntry->getValue(),
            pathVariables,
            scope
        );

        unique_ptr<TypedExpression> headers;
        unique_ptr<TypedExpression> customHeaders;
        unique_ptr<TypedExpression> queryParams;
        unique_ptr<TypedExpression> body;
        unique_ptr<TypedExpression> timeout;
        unique_ptr<TypedExpression> retryPolicy;
        unique_ptr<TypedExpression> authentication;
        unique_ptr<TypedExpression> multipart;
        unique_ptr<TypedExpression> uploadProgress;
        unique_ptr<TypedExpression> downloadStreaming;
        unique_ptr<TypedExpression> coalesce;
        unique_ptr<TypedExpression> proxy;
        unique_ptr<TypedExpression> certificatePolicy;
        unique_ptr<TypedExpression> telemetry;
        const ast::CrossaRequestEntry* headersEntry =
            findRequestEntry(expression, "headers");
        if (headersEntry != nullptr) {
            headers = analyzeExpression(headersEntry->getValue(), scope);
            validateRequestMap(*headers, "headers");
        }
        const ast::CrossaRequestEntry* customHeadersEntry =
            findRequestEntry(expression, "customHeaders");
        if (customHeadersEntry != nullptr) {
            customHeaders = analyzeExpression(
                customHeadersEntry->getValue(),
                scope
            );
            validateRequestMap(*customHeaders, "customHeaders");
        }
        const ast::CrossaRequestEntry* queryEntry =
            findRequestEntry(expression, "queryParams");
        if (queryEntry != nullptr) {
            queryParams = analyzeExpression(queryEntry->getValue(), scope);
            validateRequestMap(*queryParams, "queryParams");
        }
        const ast::CrossaRequestEntry* bodyEntry =
            findRequestEntry(expression, "body");
        if (bodyEntry != nullptr) {
            body = analyzeExpression(bodyEntry->getValue(), scope);
            if (body->getType().getKind() ==
                    types::SemanticTypeKind::Unit ||
                body->getType().getKind() ==
                    types::SemanticTypeKind::Model ||
                body->getType().getKind() ==
                    types::SemanticTypeKind::List) {
                fail(
                    bodyEntry->getLocation(),
                    "CRA7007",
                    "CrossaRequest body requires a JSON-compatible value."
                );
            }
        }
        const ast::CrossaRequestEntry* timeoutEntry =
            findRequestEntry(expression, "timeout");
        if (timeoutEntry != nullptr) {
            timeout = analyzeExpression(timeoutEntry->getValue(), scope);
            if (timeout->getType().getKind() !=
                types::SemanticTypeKind::Int) {
                fail(
                    timeoutEntry->getLocation(),
                    "CRA7008",
                    "CrossaRequest timeout expects Int milliseconds."
                );
            }
        }

        const ast::CrossaRequestEntry* retryPolicyEntry =
            findRequestEntry(expression, "retryPolicy");
        if (retryPolicyEntry != nullptr) {
            retryPolicy = analyzeExpression(
                retryPolicyEntry->getValue(),
                scope
            );
            validateRequestObject(*retryPolicy, "retryPolicy");
        }
        const ast::CrossaRequestEntry* authenticationEntry =
            findRequestEntry(expression, "auth");
        if (authenticationEntry != nullptr) {
            authentication = analyzeExpression(
                authenticationEntry->getValue(),
                scope
            );
            if (authentication->getKind() != TypedExpressionKind::JsonObject &&
                authentication->getType().getKind() !=
                    types::SemanticTypeKind::String) {
                fail(
                    authenticationEntry->getLocation(),
                    "CRA7013",
                    "CrossaRequest auth expects a provider String or JSON object."
                );
            }
        }
        const unordered_set<string> requestObjectFields{
            "multipart",
            "proxy",
            "certificatePolicy",
            "telemetry"
        };
        for (const string& fieldName : requestObjectFields) {
            const ast::CrossaRequestEntry* entry =
                findRequestEntry(expression, fieldName);
            if (entry == nullptr) {
                continue;
            }
            unique_ptr<TypedExpression> value = analyzeExpression(
                entry->getValue(),
                scope
            );
            validateRequestObject(*value, fieldName);
            if (fieldName == "multipart") {
                multipart = std::move(value);
            } else if (fieldName == "proxy") {
                proxy = std::move(value);
            } else if (fieldName == "certificatePolicy") {
                certificatePolicy = std::move(value);
            } else {
                telemetry = std::move(value);
            }
        }
        const unordered_set<string> requestBoolFields{
            "uploadProgress",
            "downloadStreaming",
            "coalesce"
        };
        for (const string& fieldName : requestBoolFields) {
            const ast::CrossaRequestEntry* entry =
                findRequestEntry(expression, fieldName);
            if (entry == nullptr) {
                continue;
            }
            unique_ptr<TypedExpression> value = analyzeExpression(
                entry->getValue(),
                scope
            );
            if (value->getType().getKind() != types::SemanticTypeKind::Bool) {
                fail(
                    entry->getLocation(),
                    "CRA7014",
                    "CrossaRequest " + fieldName + " expects Bool."
                );
            }
            if (fieldName == "uploadProgress") {
                uploadProgress = std::move(value);
            } else if (fieldName == "downloadStreaming") {
                downloadStreaming = std::move(value);
            } else {
                coalesce = std::move(value);
            }
        }

        return make_unique<TypedCrossaRequestExpression>(
            resolveHttpMethod(methodExpression.getMethod()),
            std::move(url),
            std::move(headers),
            std::move(customHeaders),
            std::move(queryParams),
            std::move(body),
            std::move(timeout),
            std::move(retryPolicy),
            std::move(authentication),
            std::move(multipart),
            std::move(uploadProgress),
            std::move(downloadStreaming),
            std::move(coalesce),
            std::move(proxy),
            std::move(certificatePolicy),
            std::move(telemetry),
            responseType,
            expression.getLocation()
        );
    }

    // Resolves URL interpolation including explicit path-variable aliases.
    unique_ptr<TypedExpression> SemanticAnalyzer::analyzeRequestUrl(
        const ast::Expression& expression,
        const ast::JsonObjectExpression* pathVariables,
        const SemanticScope& scope
    ) {
        if (expression.getKind() != ast::ExpressionKind::StringLiteral) {
            fail(
                expression.getLocation(),
                "CRA7005",
                "CrossaRequest url expects a String literal."
            );
        }
        const auto& stringExpression =
            static_cast<const ast::StringLiteralExpression&>(expression);
        vector<TypedStringSegment> segments;
        segments.reserve(stringExpression.getSegments().size());
        unordered_set<string> usedAliases;

        for (const ast::StringSegment& segment :
             stringExpression.getSegments()) {
            if (segment.getKind() == ast::StringSegmentKind::Literal) {
                segments.emplace_back(
                    TypedStringSegmentKind::Literal,
                    segment.getValue(),
                    types::SemanticType::createString(),
                    nullopt,
                    segment.getLocation()
                );
                continue;
            }

            string symbolName = segment.getValue();
            const ValueSymbol* symbol = scope.resolve(symbolName);
            if (symbol == nullptr && pathVariables != nullptr) {
                for (const ast::JsonObjectEntry& entry :
                     pathVariables->getEntries()) {
                    if (entry.getKey() != segment.getValue()) {
                        continue;
                    }
                    usedAliases.insert(entry.getKey());
                    if (entry.getValue().getKind() !=
                        ast::ExpressionKind::Identifier) {
                        fail(
                            entry.getLocation(),
                            "CRA7009",
                            "Path variable aliases must reference an identifier."
                        );
                    }
                    symbolName = static_cast<
                        const ast::IdentifierExpression&
                    >(entry.getValue()).getName();
                    symbol = scope.resolve(symbolName);
                    break;
                }
            }
            if (symbol == nullptr) {
                fail(
                    segment.getLocation(),
                    "CRA2006",
                    "Unknown request path variable '" +
                    segment.getValue() + "'."
                );
            }
            const types::SemanticTypeKind kind = symbol->getType().getKind();
            if (kind != types::SemanticTypeKind::Int &&
                kind != types::SemanticTypeKind::Long &&
                kind != types::SemanticTypeKind::Double &&
                kind != types::SemanticTypeKind::String &&
                kind != types::SemanticTypeKind::Bool) {
                fail(
                    segment.getLocation(),
                    "CRA7009",
                    "Request path variables require Int, Long, Double, "
                    "String, or Bool."
                );
            }
            segments.emplace_back(
                TypedStringSegmentKind::Symbol,
                symbolName,
                symbol->getType(),
                symbol->getKind(),
                segment.getLocation()
            );
        }

        if (pathVariables != nullptr) {
            for (const ast::JsonObjectEntry& entry :
                 pathVariables->getEntries()) {
                if (!usedAliases.contains(entry.getKey()) &&
                    scope.resolve(entry.getKey()) == nullptr) {
                    fail(
                        entry.getLocation(),
                        "CRA7009",
                        "Unused path variable alias '" + entry.getKey() + "'."
                    );
                }
            }
        }
        return make_unique<TypedStringLiteralExpression>(
            std::move(segments),
            expression.getLocation()
        );
    }

    // Ensures a request map is a JSON object containing scalar values.
    void SemanticAnalyzer::validateRequestMap(
        const TypedExpression& expression,
        const string& fieldName
    ) const {
        if (expression.getKind() != TypedExpressionKind::JsonObject) {
            fail(
                expression.getLocation(),
                "CRA7006",
                "CrossaRequest " + fieldName + " expects a JSON object."
            );
        }
        const auto& object =
            static_cast<const TypedJsonObjectExpression&>(expression);
        for (const TypedJsonObjectEntry& entry : object.getEntries()) {
            const TypedExpressionKind kind = entry.getValue().getKind();
            const types::SemanticTypeKind typeKind =
                entry.getValue().getType().getKind();
            if (kind == TypedExpressionKind::JsonObject ||
                kind == TypedExpressionKind::JsonArray ||
                kind == TypedExpressionKind::JsonNull ||
                kind == TypedExpressionKind::CrossaRequest ||
                typeKind == types::SemanticTypeKind::Unit ||
                typeKind == types::SemanticTypeKind::Model ||
                typeKind == types::SemanticTypeKind::List) {
                fail(
                    entry.getLocation(),
                    "CRA7006",
                    "CrossaRequest " + fieldName +
                    " values must be scalar."
                );
            }
        }
    }

    void SemanticAnalyzer::validateRequestObject(
        const TypedExpression& expression,
        const string& fieldName
    ) const {
        if (expression.getKind() != TypedExpressionKind::JsonObject) {
            fail(
                expression.getLocation(),
                "CRA7015",
                "CrossaRequest " + fieldName +
                " expects a JSON object."
            );
        }
    }

    // Returns one named request entry or null when it is absent.
    const ast::CrossaRequestEntry* SemanticAnalyzer::findRequestEntry(
        const ast::CrossaRequestExpression& expression,
        const string& name
    ) noexcept {
        for (const ast::CrossaRequestEntry& entry : expression.getEntries()) {
            if (entry.getName() == name) {
                return &entry;
            }
        }
        return nullptr;
    }

    // Converts an AST HTTP method into its semantic request method.
    SemanticHttpMethod SemanticAnalyzer::resolveHttpMethod(
        ast::HttpMethod method
    ) noexcept {
        switch (method) {
            case ast::HttpMethod::Get:
                return SemanticHttpMethod::Get;
            case ast::HttpMethod::Post:
                return SemanticHttpMethod::Post;
            case ast::HttpMethod::Put:
                return SemanticHttpMethod::Put;
            case ast::HttpMethod::Patch:
                return SemanticHttpMethod::Patch;
            case ast::HttpMethod::Delete:
                return SemanticHttpMethod::Delete;
            case ast::HttpMethod::Head:
                return SemanticHttpMethod::Head;
            case ast::HttpMethod::Options:
                return SemanticHttpMethod::Options;
            case ast::HttpMethod::Trace:
                return SemanticHttpMethod::Trace;
            case ast::HttpMethod::Connect:
                return SemanticHttpMethod::Connect;
        }
        return SemanticHttpMethod::Get;
    }

    // Resolves a syntax type reference into a complete semantic type.
    types::SemanticType SemanticAnalyzer::resolveType(
        const ast::TypeReference& typeReference
    ) const {
        if (typeReference.getKind() == ast::TypeReferenceKind::List) {
            const ast::TypeReference* elementType =
                typeReference.getElementType();
            if (elementType == nullptr) {
                fail(
                    typeReference.getLocation(),
                    "CRA9006",
                    "List type is missing its element type."
                );
            }
            return types::SemanticType::createList(resolveType(*elementType));
        }

        const string& name = typeReference.getName();
        if (name == "Int") {
            return types::SemanticType::createInt();
        }
        if (name == "Long") {
            return types::SemanticType::createLong();
        }
        if (name == "Double") {
            return types::SemanticType::createDouble();
        }
        if (name == "String") {
            return types::SemanticType::createString();
        }
        if (name == "Bool") {
            return types::SemanticType::createBool();
        }
        if (name == "Json") {
            return types::SemanticType::createJson();
        }
        if (modelNames_.contains(name)) {
            return types::SemanticType::createModel(name);
        }

        fail(
            typeReference.getLocation(),
            "CRA3013",
            "Unknown Crossa type '" + name + "'."
        );
    }

    // Converts an AST execution annotation into its semantic policy.
    SemanticExecutionPolicy SemanticAnalyzer::resolveExecutionPolicy(
        ast::ExecutionPolicy executionPolicy
    ) noexcept {
        switch (executionPolicy) {
            case ast::ExecutionPolicy::None:
            case ast::ExecutionPolicy::Sync:
                return SemanticExecutionPolicy::Sync;
            case ast::ExecutionPolicy::Async:
                return SemanticExecutionPolicy::Async;
            case ast::ExecutionPolicy::AsyncAfter:
                return SemanticExecutionPolicy::AsyncAfter;
        }

        return SemanticExecutionPolicy::Sync;
    }

    // Returns the stable global name represented by one declaration.
    const string* SemanticAnalyzer::getDeclarationName(
        const ast::Declaration& declaration
    ) noexcept {
        switch (declaration.getKind()) {
            case ast::DeclarationKind::Import:
                return nullptr;
            case ast::DeclarationKind::Variable:
                return &static_cast<const ast::VariableDeclaration&>(declaration)
                            .getName();
            case ast::DeclarationKind::Model:
                return &static_cast<const ast::ModelDeclaration&>(declaration)
                            .getName();
            case ast::DeclarationKind::Function:
                return &static_cast<const ast::FunctionDeclaration&>(declaration)
                            .getName();
            case ast::DeclarationKind::Config:
                return nullptr;
            case ast::DeclarationKind::Expression:
                return nullptr;
        }

        return nullptr;
    }

    // Returns the expected type for one supported config key.
    optional<types::SemanticType> SemanticAnalyzer::getConfigType(
        const string& name
    ) {
        if (name == "packageName") {
            return types::SemanticType::createString();
        }
        if (name == "baseUrl") {
            return types::SemanticType::createString();
        }
        if (name == "timeoutRequest") {
            return types::SemanticType::createInt();
        }
        if (name == "interceptor") {
            return types::SemanticType::createJson();
        }
        if (name == "commonHeaders") {
            return types::SemanticType::createJson();
        }
        if (name == "workerThreads" ||
            name == "maxQueuedTasks" ||
            name == "maxResponseBytes" ||
            name == "maxJsonDepth") {
            return types::SemanticType::createInt();
        }
        if (name == "followRedirects") {
            return types::SemanticType::createBool();
        }
        if (name == "retryPolicy" ||
            name == "authProviders" ||
            name == "proxy" ||
            name == "certificatePolicy" ||
            name == "telemetry") {
            return types::SemanticType::createJson();
        }
        if (name == "defaultAuthProvider") {
            return types::SemanticType::createString();
        }
        if (name == "uploadProgress" ||
            name == "downloadStreaming" ||
            name == "requestCoalescing") {
            return types::SemanticType::createBool();
        }

        return nullopt;
    }

    // Returns whether this type can participate in arithmetic expressions.
    bool SemanticAnalyzer::isNumericType(
        const types::SemanticType& type
    ) noexcept {
        return type.getKind() == types::SemanticTypeKind::Int ||
               type.getKind() == types::SemanticTypeKind::Long ||
               type.getKind() == types::SemanticTypeKind::Double;
    }

    // Throws a deterministic source-aware semantic diagnostic.
    [[noreturn]] void SemanticAnalyzer::fail(
        const source::SourceLocation& location,
        const string& code,
        const string& message
    ) const {
        const string_view locationSourcePath = location.getSourcePath();
        const string diagnosticPath = locationSourcePath.empty()
            ? sourceFile_.getPath().string()
            : string(locationSourcePath);
        throw runtime_error(
            diagnosticPath + ":" +
            to_string(location.getLine()) + ":" +
            to_string(location.getColumn()) + ": " + code + " " + message
        );
    }

}
