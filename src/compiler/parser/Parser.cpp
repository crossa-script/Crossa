#include "crossa/compiler/parser/Parser.h"

#include <optional>
#include <stdexcept>
#include <utility>

using namespace std;

namespace crossa::compiler::parser {

    // Creates a parser over one token stream and its owning source file.
    Parser::Parser(
        const vector<lexer::Token>& tokens,
        const source::SourceFile& sourceFile
    )
        : tokens_(tokens),
          sourceFile_(sourceFile),
          sourcePath_(make_shared<const string>(sourceFile.getPath().string())),
          current_(0) {}

    // Parses the complete token stream into one AST source unit.
    ast::SourceUnit Parser::parse() {
        if (tokens_.empty()) {
            throw runtime_error(
                sourceFile_.getPath().string() +
                ":1:1: Parser received an empty token stream."
            );
        }
        if (tokens_.back().getType() != lexer::TokenType::EndOfFile) {
            throw runtime_error(
                sourceFile_.getPath().string() +
                ":1:1: Parser token stream is missing EndOfFile."
            );
        }

        vector<unique_ptr<ast::Declaration>> declarations;
        bool declarationsStarted = false;
        while (!isAtEnd()) {
            if (match(lexer::TokenType::KeywordImport)) {
                if (declarationsStarted) {
                    fail(
                        previous(),
                        "Imports must appear before all declarations."
                    );
                }
                declarations.push_back(parseImportDeclaration());
                continue;
            }

            declarationsStarted = true;
            declarations.push_back(parseDeclaration());
        }

        return ast::SourceUnit(std::move(declarations));
    }

    // Parses one top-level import after consuming the import keyword.
    unique_ptr<ast::Declaration> Parser::parseImportDeclaration() {
        const lexer::Token& importToken = previous();
        const lexer::Token& pathToken = consume(
            lexer::TokenType::ImportPath,
            "Expected a hash-delimited .cra filename after 'import'."
        );
        const string lexeme = getLexeme(pathToken);
        if (lexeme.size() < 3) {
            fail(pathToken, "Import filename cannot be empty.");
        }

        const string filename = lexeme.substr(1, lexeme.size() - 2);
        if (filename.find('/') != string::npos ||
            filename.find('\\') != string::npos) {
            fail(
                pathToken,
                "Import accepts a filename only; paths are not supported."
            );
        }
        for (const char value : filename) {
            const bool supported =
                (value >= 'a' && value <= 'z') ||
                (value >= 'A' && value <= 'Z') ||
                (value >= '0' && value <= '9') ||
                value == '_' || value == '-' || value == '.';
            if (!supported) {
                fail(
                    pathToken,
                    "Import filename contains an unsupported character."
                );
            }
        }
        if (filename == "." || filename == ".." ||
            filename.size() <= 4 ||
            filename.substr(filename.size() - 4) != ".cra") {
            fail(pathToken, "Import filename must end with '.cra'.");
        }

        return make_unique<ast::ImportDeclaration>(
            filename,
            getLocation(importToken)
        );
    }

    // Parses one top-level declaration.
    unique_ptr<ast::Declaration> Parser::parseDeclaration() {
        if (match(lexer::TokenType::AnnotationSync)) {
            return parseAnnotatedFunction(ast::ExecutionPolicy::Sync);
        }
        if (match(lexer::TokenType::AnnotationAsync)) {
            return parseAnnotatedFunction(ast::ExecutionPolicy::Async);
        }
        if (match(lexer::TokenType::AnnotationAsyncAfter)) {
            return parseAnnotatedFunction(ast::ExecutionPolicy::AsyncAfter);
        }
        if (match(lexer::TokenType::KeywordFun)) {
            return parseFunctionDeclaration(ast::ExecutionPolicy::None);
        }
        if (match(lexer::TokenType::KeywordVar)) {
            return parseVariableDeclaration();
        }
        if (match(lexer::TokenType::KeywordModel)) {
            return parseModelDeclaration();
        }
        if (match(lexer::TokenType::KeywordConfig)) {
            return parseConfigDeclaration();
        }

        unique_ptr<ast::Expression> expression = parseExpression();
        const source::SourceLocation location = expression->getLocation();
        return make_unique<ast::ExpressionDeclaration>(
            std::move(expression),
            location
        );
    }

    // Parses a function preceded by one execution annotation.
    unique_ptr<ast::Declaration> Parser::parseAnnotatedFunction(
        ast::ExecutionPolicy executionPolicy
    ) {
        if (check(lexer::TokenType::AnnotationSync) ||
            check(lexer::TokenType::AnnotationAsync) ||
            check(lexer::TokenType::AnnotationAsyncAfter)) {
            fail(peek(), "A function accepts only one execution annotation.");
        }

        consume(
            lexer::TokenType::KeywordFun,
            "Expected 'fun' after the execution annotation."
        );
        return parseFunctionDeclaration(executionPolicy);
    }

    // Parses a function declaration after consuming the fun keyword.
    unique_ptr<ast::Declaration> Parser::parseFunctionDeclaration(
        ast::ExecutionPolicy executionPolicy
    ) {
        const lexer::Token& nameToken = consume(
            lexer::TokenType::Identifier,
            "Expected a function name after 'fun'."
        );
        consume(lexer::TokenType::LeftParen, "Expected '(' after the function name.");

        vector<ast::Parameter> parameters;
        if (!check(lexer::TokenType::RightParen)) {
            do {
                const lexer::Token& parameterName = consume(
                    lexer::TokenType::Identifier,
                    "Expected a parameter name."
                );
                consume(
                    lexer::TokenType::Colon,
                    "Expected ':' after the parameter name."
                );
                parameters.emplace_back(
                    getLexeme(parameterName),
                    parseTypeReference(),
                    getLocation(parameterName)
                );
            } while (match(lexer::TokenType::Comma));
        }

        consume(
            lexer::TokenType::RightParen,
            "Expected ')' after the function parameters."
        );

        optional<ast::TypeReference> returnType;
        if (match(lexer::TokenType::Colon)) {
            returnType.emplace(parseTypeReference());
        }

        consume(lexer::TokenType::LeftBrace, "Expected '{' before the function body.");
        vector<unique_ptr<ast::Statement>> statements = parseBlockStatements();

        return make_unique<ast::FunctionDeclaration>(
            getLexeme(nameToken),
            executionPolicy,
            std::move(parameters),
            std::move(returnType),
            std::move(statements),
            getLocation(nameToken)
        );
    }

    // Parses a top-level variable after consuming the var keyword.
    unique_ptr<ast::Declaration> Parser::parseVariableDeclaration() {
        const lexer::Token& nameToken = consume(
            lexer::TokenType::Identifier,
            "Expected a variable name after 'var'."
        );
        consume(lexer::TokenType::Colon, "Expected ':' after the variable name.");
        ast::TypeReference type = parseTypeReference();
        consume(lexer::TokenType::Equal, "Expected '=' before the variable initializer.");
        unique_ptr<ast::Expression> initializer = parseExpression();

        return make_unique<ast::VariableDeclaration>(
            getLexeme(nameToken),
            std::move(type),
            std::move(initializer),
            getLocation(nameToken)
        );
    }

    // Parses a model declaration after consuming the model keyword.
    unique_ptr<ast::Declaration> Parser::parseModelDeclaration() {
        const lexer::Token& nameToken = consume(
            lexer::TokenType::Identifier,
            "Expected a model name after 'model'."
        );
        consume(lexer::TokenType::LeftParen, "Expected '(' after the model name.");

        vector<ast::ModelField> fields;
        if (!check(lexer::TokenType::RightParen)) {
            do {
                const lexer::Token& fieldName = consume(
                    lexer::TokenType::Identifier,
                    "Expected a model field name."
                );
                consume(
                    lexer::TokenType::Colon,
                    "Expected ':' after the model field name."
                );
                fields.emplace_back(
                    getLexeme(fieldName),
                    parseTypeReference(),
                    getLocation(fieldName)
                );
            } while (match(lexer::TokenType::Comma));
        }

        consume(lexer::TokenType::RightParen, "Expected ')' after model fields.");
        return make_unique<ast::ModelDeclaration>(
            getLexeme(nameToken),
            std::move(fields),
            getLocation(nameToken)
        );
    }

    // Parses a config declaration after consuming the config keyword.
    unique_ptr<ast::Declaration> Parser::parseConfigDeclaration() {
        const lexer::Token& configToken = previous();
        consume(lexer::TokenType::LeftBrace, "Expected '{' after 'config'.");

        vector<ast::ConfigEntry> entries;
        if (!check(lexer::TokenType::RightBrace)) {
            do {
                const lexer::Token& entryName = consume(
                    lexer::TokenType::Identifier,
                    "Expected a configuration key."
                );
                consume(
                    lexer::TokenType::Colon,
                    "Expected ':' after the configuration key."
                );
                entries.emplace_back(
                    getLexeme(entryName),
                    parseExpression(),
                    getLocation(entryName)
                );
            } while (match(lexer::TokenType::Comma));
        }

        consume(lexer::TokenType::RightBrace, "Expected '}' after config entries.");
        return make_unique<ast::ConfigDeclaration>(
            std::move(entries),
            getLocation(configToken)
        );
    }

    // Parses one function-body statement.
    unique_ptr<ast::Statement> Parser::parseStatement() {
        if (match(lexer::TokenType::KeywordRe)) {
            const source::SourceLocation location = getLocation(previous());
            return make_unique<ast::ReturnStatement>(
                parseExpression(),
                location
            );
        }
        if (match(lexer::TokenType::KeywordVar)) {
            return parseVariableStatement();
        }
        if (match(lexer::TokenType::KeywordIf)) {
            return parseIfStatement(getLocation(previous()));
        }

        unique_ptr<ast::Expression> expression = parseExpression();
        const source::SourceLocation location = expression->getLocation();
        return make_unique<ast::ExpressionStatement>(
            std::move(expression),
            location
        );
    }

    unique_ptr<ast::Statement> Parser::parseIfStatement(
        source::SourceLocation location
    ) {
        consume(lexer::TokenType::LeftParen, "Expected '(' after 'if'.");
        unique_ptr<ast::Expression> condition = parseExpression();
        consume(lexer::TokenType::RightParen, "Expected ')' after the if condition.");
        consume(lexer::TokenType::LeftBrace, "Expected '{' after the if condition.");
        vector<unique_ptr<ast::Statement>> thenStatements = parseBlockStatements();

        optional<vector<unique_ptr<ast::Statement>>> elseStatements;
        if (match(lexer::TokenType::KeywordElse)) {
            if (match(lexer::TokenType::KeywordIf)) {
                vector<unique_ptr<ast::Statement>> nestedIf;
                nestedIf.push_back(parseIfStatement(getLocation(previous())));
                elseStatements.emplace(std::move(nestedIf));
            } else {
                consume(lexer::TokenType::LeftBrace, "Expected '{' after 'else'.");
                elseStatements.emplace(parseBlockStatements());
            }
        }

        return make_unique<ast::IfStatement>(
            std::move(condition),
            std::move(thenStatements),
            std::move(elseStatements),
            location
        );
    }

    vector<unique_ptr<ast::Statement>> Parser::parseBlockStatements() {
        vector<unique_ptr<ast::Statement>> statements;
        while (!check(lexer::TokenType::RightBrace) && !isAtEnd()) {
            statements.push_back(parseStatement());
        }
        consume(lexer::TokenType::RightBrace, "Expected '}' after the block.");
        return statements;
    }

    // Parses a local variable after consuming the var keyword.
    unique_ptr<ast::Statement> Parser::parseVariableStatement() {
        const lexer::Token& nameToken = consume(
            lexer::TokenType::Identifier,
            "Expected a local variable name after 'var'."
        );
        consume(lexer::TokenType::Colon, "Expected ':' after the variable name.");
        ast::TypeReference type = parseTypeReference();
        consume(lexer::TokenType::Equal, "Expected '=' before the variable initializer.");

        return make_unique<ast::VariableStatement>(
            getLexeme(nameToken),
            std::move(type),
            parseExpression(),
            getLocation(nameToken)
        );
    }

    // Parses one type reference including nested List<T> forms.
    ast::TypeReference Parser::parseTypeReference() {
        if (match(lexer::TokenType::KeywordList)) {
            const source::SourceLocation location = getLocation(previous());
            consume(lexer::TokenType::LeftAngle, "Expected '<' after 'List'.");
            ast::TypeReference elementType = parseTypeReference();
            consume(lexer::TokenType::RightAngle, "Expected '>' after the List type.");
            return ast::TypeReference::createList(
                std::move(elementType),
                location
            );
        }

        if (match(lexer::TokenType::KeywordInt) ||
            match(lexer::TokenType::KeywordLong) ||
            match(lexer::TokenType::KeywordDouble) ||
            match(lexer::TokenType::KeywordString) ||
            match(lexer::TokenType::KeywordBool) ||
            match(lexer::TokenType::KeywordJson) ||
            match(lexer::TokenType::Identifier)) {
            return ast::TypeReference::createNamed(
                getLexeme(previous()),
                getLocation(previous())
            );
        }

        fail(peek(), "Expected a Crossa type reference.");
    }

    // Parses one expression using arithmetic precedence.
    unique_ptr<ast::Expression> Parser::parseExpression() {
        return parseEqualityExpression();
    }

    unique_ptr<ast::Expression> Parser::parseEqualityExpression() {
        unique_ptr<ast::Expression> expression = parseComparisonExpression();
        while (check(lexer::TokenType::EqualEqual) ||
               check(lexer::TokenType::BangEqual)) {
            const lexer::Token& operatorToken = advance();
            const ast::BinaryOperator operation =
                operatorToken.getType() == lexer::TokenType::EqualEqual
                    ? ast::BinaryOperator::Equal
                    : ast::BinaryOperator::NotEqual;
            expression = make_unique<ast::BinaryExpression>(
                std::move(expression),
                operation,
                parseComparisonExpression(),
                getLocation(operatorToken)
            );
        }
        return expression;
    }

    unique_ptr<ast::Expression> Parser::parseComparisonExpression() {
        unique_ptr<ast::Expression> expression = parseAdditiveExpression();
        while (check(lexer::TokenType::LeftAngle) ||
               check(lexer::TokenType::RightAngle) ||
               check(lexer::TokenType::LessEqual) ||
               check(lexer::TokenType::GreaterEqual)) {
            const lexer::Token& operatorToken = advance();
            ast::BinaryOperator operation = ast::BinaryOperator::Less;
            switch (operatorToken.getType()) {
                case lexer::TokenType::LeftAngle:
                    operation = ast::BinaryOperator::Less;
                    break;
                case lexer::TokenType::RightAngle:
                    operation = ast::BinaryOperator::Greater;
                    break;
                case lexer::TokenType::LessEqual:
                    operation = ast::BinaryOperator::LessEqual;
                    break;
                case lexer::TokenType::GreaterEqual:
                    operation = ast::BinaryOperator::GreaterEqual;
                    break;
                default:
                    fail(operatorToken, "Expected a comparison operator.");
            }
            expression = make_unique<ast::BinaryExpression>(
                std::move(expression),
                operation,
                parseAdditiveExpression(),
                getLocation(operatorToken)
            );
        }
        return expression;
    }

    // Parses addition and subtraction expressions.
    unique_ptr<ast::Expression> Parser::parseAdditiveExpression() {
        unique_ptr<ast::Expression> expression = parseMultiplicativeExpression();

        while (check(lexer::TokenType::Plus) ||
               check(lexer::TokenType::Minus)) {
            const lexer::Token& operatorToken = advance();
            const lexer::TokenType operation = operatorToken.getType();
            const ast::BinaryOperator binaryOperator =
                operation == lexer::TokenType::Plus
                    ? ast::BinaryOperator::Add
                    : ast::BinaryOperator::Subtract;
            expression = make_unique<ast::BinaryExpression>(
                std::move(expression),
                binaryOperator,
                parseMultiplicativeExpression(),
                getLocation(operatorToken)
            );
        }

        return expression;
    }

    // Parses multiplication and division expressions.
    unique_ptr<ast::Expression> Parser::parseMultiplicativeExpression() {
        unique_ptr<ast::Expression> expression = parseUnaryExpression();

        while (check(lexer::TokenType::Star) ||
               check(lexer::TokenType::Slash)) {
            const lexer::Token& operatorToken = advance();
            const lexer::TokenType operation = operatorToken.getType();
            const ast::BinaryOperator binaryOperator =
                operation == lexer::TokenType::Star
                    ? ast::BinaryOperator::Multiply
                    : ast::BinaryOperator::Divide;
            expression = make_unique<ast::BinaryExpression>(
                std::move(expression),
                binaryOperator,
                parseUnaryExpression(),
                getLocation(operatorToken)
            );
        }

        return expression;
    }

    // Parses unary negation expressions.
    unique_ptr<ast::Expression> Parser::parseUnaryExpression() {
        if (match(lexer::TokenType::Minus)) {
            const source::SourceLocation location = getLocation(previous());
            if (match(lexer::TokenType::DecimalLiteral)) {
                return make_unique<ast::UnaryExpression>(
                    ast::UnaryOperator::Negate,
                    make_unique<ast::DecimalLiteralExpression>(
                        getLexeme(previous()),
                        getLocation(previous())
                    ),
                    location
                );
            }
            if (match(lexer::TokenType::IntegerLiteral)) {
                return make_unique<ast::UnaryExpression>(
                    ast::UnaryOperator::Negate,
                    make_unique<ast::IntegerLiteralExpression>(
                        getLexeme(previous()),
                        getLocation(previous())
                    ),
                    location
                );
            }
            return make_unique<ast::UnaryExpression>(
                ast::UnaryOperator::Negate,
                parseUnaryExpression(),
                location
            );
        }

        return parsePrimaryExpression();
    }

    // Parses literals, identifiers, calls, and grouped expressions.
    unique_ptr<ast::Expression> Parser::parsePrimaryExpression() {
        if (match(lexer::TokenType::IntegerLiteral)) {
            const lexer::Token& token = previous();
            return make_unique<ast::IntegerLiteralExpression>(
                getLexeme(token),
                getLocation(token)
            );
        }

        if (match(lexer::TokenType::DecimalLiteral)) {
            const lexer::Token& token = previous();
            return make_unique<ast::DecimalLiteralExpression>(
                getLexeme(token),
                getLocation(token)
            );
        }

        if (match(lexer::TokenType::StringLiteral)) {
            const lexer::Token& token = previous();
            return make_unique<ast::StringLiteralExpression>(
                parseStringSegments(token),
                getLocation(token)
            );
        }

        if (match(lexer::TokenType::BooleanLiteral)) {
            const lexer::Token& token = previous();
            return make_unique<ast::BooleanLiteralExpression>(
                getLexeme(token) == "true",
                getLocation(token)
            );
        }

        if (match(lexer::TokenType::KeywordNull)) {
            return make_unique<ast::JsonNullExpression>(
                getLocation(previous())
            );
        }

        if (match(lexer::TokenType::KeywordCrossaRequest)) {
            return parseCrossaRequestExpression(getLocation(previous()));
        }

        if (check(lexer::TokenType::MethodGet) ||
            check(lexer::TokenType::MethodPost) ||
            check(lexer::TokenType::MethodPut) ||
            check(lexer::TokenType::MethodPatch) ||
            check(lexer::TokenType::MethodDelete) ||
            check(lexer::TokenType::MethodHead) ||
            check(lexer::TokenType::MethodOptions) ||
            check(lexer::TokenType::MethodTrace) ||
            check(lexer::TokenType::MethodConnect)) {
            return parseHttpMethodExpression();
        }

        if (match(lexer::TokenType::LeftBrace)) {
            return parseJsonObjectExpression(getLocation(previous()));
        }

        if (match(lexer::TokenType::LeftBracket)) {
            return parseJsonArrayExpression(getLocation(previous()));
        }

        if (match(lexer::TokenType::KeywordPrint)) {
            const source::SourceLocation location = getLocation(previous());
            consume(lexer::TokenType::LeftParen, "Expected '(' after 'print'.");
            return parseCallExpression("print", location);
        }

        if (match(lexer::TokenType::KeywordAssert)) {
            const source::SourceLocation location = getLocation(previous());
            consume(lexer::TokenType::LeftParen, "Expected '(' after 'assert'.");
            return parseCallExpression("assert", location);
        }

        if (match(lexer::TokenType::Identifier)) {
            const lexer::Token& identifierToken = previous();
            const string name = getLexeme(identifierToken);
            if (match(lexer::TokenType::LeftParen)) {
                return parseCallExpression(name, getLocation(identifierToken));
            }
            return make_unique<ast::IdentifierExpression>(
                name,
                getLocation(identifierToken)
            );
        }

        if (match(lexer::TokenType::LeftParen)) {
            unique_ptr<ast::Expression> expression = parseExpression();
            consume(lexer::TokenType::RightParen, "Expected ')' after the expression.");
            return expression;
        }

        fail(peek(), "Expected an expression.");
    }

    // Parses a CrossaRequest builder after consuming its keyword.
    unique_ptr<ast::Expression> Parser::parseCrossaRequestExpression(
        source::SourceLocation location
    ) {
        consume(
            lexer::TokenType::LeftBrace,
            "Expected '{' after 'CrossaRequest'."
        );
        vector<ast::CrossaRequestEntry> entries;
        if (!check(lexer::TokenType::RightBrace)) {
            do {
                const lexer::Token& nameToken = consume(
                    lexer::TokenType::Identifier,
                    "Expected a CrossaRequest entry name."
                );
                consume(
                    lexer::TokenType::Colon,
                    "Expected ':' after the CrossaRequest entry name."
                );
                entries.emplace_back(
                    getLexeme(nameToken),
                    parseExpression(),
                    getLocation(nameToken)
                );
            } while (match(lexer::TokenType::Comma));
        }
        consume(
            lexer::TokenType::RightBrace,
            "Expected '}' after CrossaRequest entries."
        );
        return make_unique<ast::CrossaRequestExpression>(
            std::move(entries),
            location
        );
    }

    // Parses a JSON object after consuming its left brace.
    unique_ptr<ast::Expression> Parser::parseJsonObjectExpression(
        source::SourceLocation location
    ) {
        vector<ast::JsonObjectEntry> entries;
        if (!check(lexer::TokenType::RightBrace)) {
            do {
                const source::SourceLocation entryLocation =
                    getLocation(peek());
                string key = parseJsonObjectKey();
                consume(
                    lexer::TokenType::Colon,
                    "Expected ':' after the JSON object key."
                );
                entries.emplace_back(
                    std::move(key),
                    parseExpression(),
                    entryLocation
                );
            } while (match(lexer::TokenType::Comma));
        }
        consume(
            lexer::TokenType::RightBrace,
            "Expected '}' after the JSON object."
        );
        return make_unique<ast::JsonObjectExpression>(
            std::move(entries),
            location
        );
    }

    // Parses a JSON array after consuming its left bracket.
    unique_ptr<ast::Expression> Parser::parseJsonArrayExpression(
        source::SourceLocation location
    ) {
        vector<unique_ptr<ast::Expression>> values;
        if (!check(lexer::TokenType::RightBracket)) {
            do {
                values.push_back(parseExpression());
            } while (match(lexer::TokenType::Comma));
        }
        consume(
            lexer::TokenType::RightBracket,
            "Expected ']' after the JSON array."
        );
        return make_unique<ast::JsonArrayExpression>(
            std::move(values),
            location
        );
    }

    // Parses one standard HTTP method literal.
    unique_ptr<ast::Expression> Parser::parseHttpMethodExpression() {
        const lexer::Token& token = advance();
        ast::HttpMethod method = ast::HttpMethod::Get;
        switch (token.getType()) {
            case lexer::TokenType::MethodGet:
                method = ast::HttpMethod::Get;
                break;
            case lexer::TokenType::MethodPost:
                method = ast::HttpMethod::Post;
                break;
            case lexer::TokenType::MethodPut:
                method = ast::HttpMethod::Put;
                break;
            case lexer::TokenType::MethodPatch:
                method = ast::HttpMethod::Patch;
                break;
            case lexer::TokenType::MethodDelete:
                method = ast::HttpMethod::Delete;
                break;
            case lexer::TokenType::MethodHead:
                method = ast::HttpMethod::Head;
                break;
            case lexer::TokenType::MethodOptions:
                method = ast::HttpMethod::Options;
                break;
            case lexer::TokenType::MethodTrace:
                method = ast::HttpMethod::Trace;
                break;
            case lexer::TokenType::MethodConnect:
                method = ast::HttpMethod::Connect;
                break;
            default:
                fail(token, "Expected a supported HTTP method.");
        }
        return make_unique<ast::HttpMethodLiteralExpression>(
            method,
            getLocation(token)
        );
    }

    // Parses an identifier or static string JSON object key.
    string Parser::parseJsonObjectKey() {
        if (match(lexer::TokenType::Identifier)) {
            return getLexeme(previous());
        }
        if (match(lexer::TokenType::StringLiteral)) {
            const lexer::Token& token = previous();
            const vector<ast::StringSegment> segments =
                parseStringSegments(token);
            string key;
            for (const ast::StringSegment& segment : segments) {
                if (segment.getKind() != ast::StringSegmentKind::Literal) {
                    fail(token, "A JSON object key cannot use interpolation.");
                }
                key += segment.getValue();
            }
            return key;
        }
        fail(peek(), "Expected an identifier or string JSON object key.");
    }

    // Parses call arguments after consuming the left parenthesis.
    unique_ptr<ast::Expression> Parser::parseCallExpression(
        string callee,
        source::SourceLocation location
    ) {
        vector<unique_ptr<ast::Expression>> arguments;
        if (!check(lexer::TokenType::RightParen)) {
            do {
                arguments.push_back(parseExpression());
            } while (match(lexer::TokenType::Comma));
        }

        consume(lexer::TokenType::RightParen, "Expected ')' after call arguments.");
        return make_unique<ast::CallExpression>(
            std::move(callee),
            std::move(arguments),
            location
        );
    }

    // Parses literal and identifier segments from one string token.
    vector<ast::StringSegment> Parser::parseStringSegments(
        const lexer::Token& token
    ) const {
        const string lexeme = getLexeme(token);
        if (lexeme.size() < 2) {
            fail(token, "Invalid string literal token.");
        }

        const string_view content(lexeme.data() + 1, lexeme.size() - 2);
        vector<ast::StringSegment> segments;
        string literal;
        size_t literalStart = 0;
        size_t current = 0;

        while (current < content.size()) {
            if (content[current] == '\\') {
                if (literal.empty()) {
                    literalStart = current;
                }
                appendStringEscape(content, current, literal, token);
                continue;
            }
            if (content[current] != '#' ||
                current + 1 >= content.size() ||
                !isIdentifierStart(content[current + 1])) {
                if (literal.empty()) {
                    literalStart = current;
                }
                literal.push_back(content[current]);
                ++current;
                continue;
            }

            if (!literal.empty()) {
                segments.emplace_back(
                    ast::StringSegmentKind::Literal,
                    std::move(literal),
                    getLocation(
                        token.getLine(),
                        token.getColumn() + 1 + literalStart
                    )
                );
                literal.clear();
            }

            const size_t identifierStart = ++current;
            while (current < content.size() &&
                   isIdentifierPart(content[current])) {
                ++current;
            }
            segments.emplace_back(
                ast::StringSegmentKind::Identifier,
                string(content.substr(identifierStart, current - identifierStart)),
                getLocation(
                    token.getLine(),
                    token.getColumn() + 1 + identifierStart
                )
            );
        }

        if (!literal.empty()) {
            segments.emplace_back(
                ast::StringSegmentKind::Literal,
                std::move(literal),
                getLocation(
                    token.getLine(),
                    token.getColumn() + 1 + literalStart
                )
            );
        }

        return segments;
    }

    // Decodes one source escape and advances past its complete byte sequence.
    void Parser::appendStringEscape(
        string_view content,
        size_t& current,
        string& output,
        const lexer::Token& token
    ) const {
        ++current;
        if (current >= content.size()) {
            fail(token, "Unterminated string escape.");
        }
        const char escaped = content[current++];
        switch (escaped) {
            case '"':
            case '\\':
            case '/':
            case '#':
                output.push_back(escaped);
                return;
            case 'b':
                output.push_back('\b');
                return;
            case 'f':
                output.push_back('\f');
                return;
            case 'n':
                output.push_back('\n');
                return;
            case 'r':
                output.push_back('\r');
                return;
            case 't':
                output.push_back('\t');
                return;
            case 'u': {
                unsigned int codePoint = readHexCodeUnit(
                    content,
                    current,
                    token
                );
                if (codePoint >= 0xD800 && codePoint <= 0xDBFF) {
                    if (current + 2 > content.size() ||
                        content[current] != '\\' ||
                        content[current + 1] != 'u') {
                        fail(token, "Invalid Unicode surrogate pair.");
                    }
                    current += 2;
                    const unsigned int low = readHexCodeUnit(
                        content,
                        current,
                        token
                    );
                    if (low < 0xDC00 || low > 0xDFFF) {
                        fail(token, "Invalid Unicode surrogate pair.");
                    }
                    codePoint = 0x10000 +
                        ((codePoint - 0xD800) << 10U) +
                        (low - 0xDC00);
                } else if (codePoint >= 0xDC00 && codePoint <= 0xDFFF) {
                    fail(token, "Unexpected low Unicode surrogate.");
                }
                appendCodePoint(codePoint, output);
                return;
            }
            default:
                fail(token, "Unsupported string escape.");
        }
    }

    // Reads one four-digit Unicode code unit from source string content.
    unsigned int Parser::readHexCodeUnit(
        string_view content,
        size_t& current,
        const lexer::Token& token
    ) const {
        if (current + 4 > content.size()) {
            fail(token, "Incomplete Unicode string escape.");
        }
        unsigned int value = 0;
        for (size_t index = 0; index < 4; ++index) {
            const char digit = content[current++];
            value <<= 4U;
            if (digit >= '0' && digit <= '9') {
                value += static_cast<unsigned int>(digit - '0');
            } else if (digit >= 'a' && digit <= 'f') {
                value += static_cast<unsigned int>(digit - 'a' + 10);
            } else if (digit >= 'A' && digit <= 'F') {
                value += static_cast<unsigned int>(digit - 'A' + 10);
            } else {
                fail(token, "Invalid hexadecimal Unicode string escape.");
            }
        }
        return value;
    }

    // Appends one Unicode code point to a UTF-8 string.
    void Parser::appendCodePoint(unsigned int codePoint, string& output) {
        if (codePoint <= 0x7F) {
            output.push_back(static_cast<char>(codePoint));
        } else if (codePoint <= 0x7FF) {
            output.push_back(static_cast<char>(0xC0 | (codePoint >> 6U)));
            output.push_back(static_cast<char>(0x80 | (codePoint & 0x3FU)));
        } else if (codePoint <= 0xFFFF) {
            output.push_back(static_cast<char>(0xE0 | (codePoint >> 12U)));
            output.push_back(static_cast<char>(
                0x80 | ((codePoint >> 6U) & 0x3FU)
            ));
            output.push_back(static_cast<char>(0x80 | (codePoint & 0x3FU)));
        } else {
            output.push_back(static_cast<char>(0xF0 | (codePoint >> 18U)));
            output.push_back(static_cast<char>(
                0x80 | ((codePoint >> 12U) & 0x3FU)
            ));
            output.push_back(static_cast<char>(
                0x80 | ((codePoint >> 6U) & 0x3FU)
            ));
            output.push_back(static_cast<char>(0x80 | (codePoint & 0x3FU)));
        }
    }

    // Returns the source text represented by one token.
    string Parser::getLexeme(const lexer::Token& token) const {
        return string(token.getLexeme(sourceFile_));
    }

    // Converts a lexer token position into an AST source location.
    source::SourceLocation Parser::getLocation(
        const lexer::Token& token
    ) const noexcept {
        return getLocation(token.getLine(), token.getColumn());
    }

    // Creates a source location inside the parser's current file.
    source::SourceLocation Parser::getLocation(
        size_t line,
        size_t column
    ) const noexcept {
        return source::SourceLocation(
            sourcePath_,
            line,
            column
        );
    }

    // Consumes one token when its type matches the expectation.
    const lexer::Token& Parser::consume(
        lexer::TokenType type,
        const string& message
    ) {
        if (check(type)) {
            return advance();
        }

        fail(peek(), message);
    }

    // Consumes and reports whether the next token has the requested type.
    bool Parser::match(lexer::TokenType type) noexcept {
        if (!check(type)) {
            return false;
        }

        advance();
        return true;
    }

    // Returns whether the current token has the requested type.
    bool Parser::check(lexer::TokenType type) const noexcept {
        return peek().getType() == type;
    }

    // Consumes and returns the current token.
    const lexer::Token& Parser::advance() noexcept {
        if (!isAtEnd()) {
            ++current_;
        }
        return previous();
    }

    // Returns whether the parser reached the EndOfFile token.
    bool Parser::isAtEnd() const noexcept {
        return peek().getType() == lexer::TokenType::EndOfFile;
    }

    // Returns the current token without consuming it.
    const lexer::Token& Parser::peek() const noexcept {
        return tokens_[current_];
    }

    // Returns the most recently consumed token.
    const lexer::Token& Parser::previous() const noexcept {
        return tokens_[current_ - 1];
    }

    // Returns whether a byte can begin an interpolation identifier.
    bool Parser::isIdentifierStart(char value) noexcept {
        return (value >= 'a' && value <= 'z') ||
               (value >= 'A' && value <= 'Z') ||
               value == '_';
    }

    // Returns whether a byte can continue an interpolation identifier.
    bool Parser::isIdentifierPart(char value) noexcept {
        return isIdentifierStart(value) ||
               (value >= '0' && value <= '9');
    }

    // Throws a deterministic parser failure at the provided token.
    [[noreturn]] void Parser::fail(
        const lexer::Token& token,
        const string& message
    ) const {
        throw runtime_error(
            sourceFile_.getPath().string() + ":" +
            to_string(token.getLine()) + ":" +
            to_string(token.getColumn()) + ": " +
            message
        );
    }

}
