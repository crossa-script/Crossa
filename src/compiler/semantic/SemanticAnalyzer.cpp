#include "crossa/compiler/semantic/SemanticAnalyzer.h"

#include <filesystem>
#include <optional>
#include <stdexcept>
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
            const string sourceIdentity =
                sourceFile_.getPath().stem().string();
            if (model.getName() != sourceIdentity) {
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
            globalScope_
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
            if (value->getType() != *expectedType) {
                fail(
                    entry.getValue().getLocation(),
                    "CRA5007",
                    "Config key '" + entry.getName() + "' expects '" +
                    expectedType->format() + "' but received '" +
                    value->getType().format() + "'."
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

                unique_ptr<TypedExpression> expression = analyzeExpression(
                    returnStatement.getExpression(),
                    scope
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
        }

        fail(
            statement.getLocation(),
            "CRA9004",
            "Unknown AST statement kind."
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
            scope
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
        switch (expression.getKind()) {
            case ast::ExpressionKind::Identifier:
                return analyzeIdentifierExpression(
                    static_cast<const ast::IdentifierExpression&>(expression),
                    scope
                );
            case ast::ExpressionKind::IntegerLiteral: {
                const auto& literal =
                    static_cast<const ast::IntegerLiteralExpression&>(expression);
                return make_unique<TypedIntegerLiteralExpression>(
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
                    scope
                );
            case ast::ExpressionKind::Binary:
                return analyzeBinaryExpression(
                    static_cast<const ast::BinaryExpression&>(expression),
                    scope
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

    // Validates one function or print builtin call.
    unique_ptr<TypedExpression> SemanticAnalyzer::analyzeCallExpression(
        const ast::CallExpression& expression,
        const SemanticScope& scope
    ) {
        vector<unique_ptr<TypedExpression>> arguments;
        arguments.reserve(expression.getArguments().size());
        for (const unique_ptr<ast::Expression>& argument :
             expression.getArguments()) {
            arguments.push_back(analyzeExpression(*argument, scope));
        }

        if (expression.getCallee() == "print") {
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
        if (arguments.size() != signature.getParameterTypes().size()) {
            fail(
                expression.getLocation(),
                "CRA3009",
                "Function '" + expression.getCallee() + "' expects " +
                to_string(signature.getParameterTypes().size()) +
                " arguments but received " + to_string(arguments.size()) + "."
            );
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
        const SemanticScope& scope
    ) {
        unique_ptr<TypedExpression> operand = analyzeExpression(
            expression.getOperand(),
            scope
        );
        if (operand->getType().getKind() != types::SemanticTypeKind::Int) {
            fail(
                operand->getLocation(),
                "CRA3011",
                "Unary '-' requires Int but received '" +
                operand->getType().format() + "'."
            );
        }

        return make_unique<TypedUnaryExpression>(
            TypedUnaryOperator::Negate,
            std::move(operand),
            types::SemanticType::createInt(),
            expression.getLocation()
        );
    }

    // Validates one binary arithmetic expression.
    unique_ptr<TypedExpression> SemanticAnalyzer::analyzeBinaryExpression(
        const ast::BinaryExpression& expression,
        const SemanticScope& scope
    ) {
        unique_ptr<TypedExpression> left = analyzeExpression(
            expression.getLeft(),
            scope
        );
        unique_ptr<TypedExpression> right = analyzeExpression(
            expression.getRight(),
            scope
        );
        if (left->getType().getKind() != types::SemanticTypeKind::Int ||
            right->getType().getKind() != types::SemanticTypeKind::Int) {
            fail(
                expression.getLocation(),
                "CRA3012",
                "Arithmetic operators require Int operands but received '" +
                left->getType().format() + "' and '" +
                right->getType().format() + "'."
            );
        }

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
        }

        return make_unique<TypedBinaryExpression>(
            std::move(left),
            operation,
            std::move(right),
            types::SemanticType::createInt(),
            expression.getLocation()
        );
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
        if (name == "String") {
            return types::SemanticType::createString();
        }
        if (name == "Bool") {
            return types::SemanticType::createBool();
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
        if (name == "baseUrl") {
            return types::SemanticType::createString();
        }
        if (name == "timeoutRequest") {
            return types::SemanticType::createInt();
        }
        if (name == "interceptor") {
            return types::SemanticType::createBool();
        }

        return nullopt;
    }

    // Throws a deterministic source-aware semantic diagnostic.
    [[noreturn]] void SemanticAnalyzer::fail(
        const source::SourceLocation& location,
        const string& code,
        const string& message
    ) const {
        throw runtime_error(
            sourceFile_.getPath().string() + ":" +
            to_string(location.getLine()) + ":" +
            to_string(location.getColumn()) + ": " + code + " " + message
        );
    }

}
