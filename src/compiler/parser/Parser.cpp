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
    ) noexcept
        : tokens_(tokens), sourceFile_(sourceFile), current_(0) {}

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
        while (!isAtEnd()) {
            declarations.push_back(parseDeclaration());
        }

        return ast::SourceUnit(std::move(declarations));
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
        vector<unique_ptr<ast::Statement>> statements;
        while (!check(lexer::TokenType::RightBrace) && !isAtEnd()) {
            statements.push_back(parseStatement());
        }
        consume(lexer::TokenType::RightBrace, "Expected '}' after the function body.");

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

        unique_ptr<ast::Expression> expression = parseExpression();
        const source::SourceLocation location = expression->getLocation();
        return make_unique<ast::ExpressionStatement>(
            std::move(expression),
            location
        );
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
            match(lexer::TokenType::KeywordString) ||
            match(lexer::TokenType::KeywordBool) ||
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
        return parseAdditiveExpression();
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

        if (match(lexer::TokenType::KeywordCrossaRequest)) {
            fail(
                previous(),
                "CrossaRequest parsing is not implemented in this milestone."
            );
        }

        if (match(lexer::TokenType::KeywordPrint)) {
            const source::SourceLocation location = getLocation(previous());
            consume(lexer::TokenType::LeftParen, "Expected '(' after 'print'.");
            return parseCallExpression("print", location);
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
        size_t literalStart = 0;
        size_t current = 0;

        while (current < content.size()) {
            if (content[current] != '#' ||
                current + 1 >= content.size() ||
                !isIdentifierStart(content[current + 1])) {
                ++current;
                continue;
            }

            if (current > literalStart) {
                segments.emplace_back(
                    ast::StringSegmentKind::Literal,
                    string(content.substr(literalStart, current - literalStart)),
                    source::SourceLocation(
                        token.getLine(),
                        token.getColumn() + 1 + literalStart
                    )
                );
            }

            const size_t identifierStart = ++current;
            while (current < content.size() &&
                   isIdentifierPart(content[current])) {
                ++current;
            }
            segments.emplace_back(
                ast::StringSegmentKind::Identifier,
                string(content.substr(identifierStart, current - identifierStart)),
                source::SourceLocation(
                    token.getLine(),
                    token.getColumn() + 1 + identifierStart
                )
            );
            literalStart = current;
        }

        if (literalStart < content.size()) {
            segments.emplace_back(
                ast::StringSegmentKind::Literal,
                string(content.substr(literalStart)),
                source::SourceLocation(
                    token.getLine(),
                    token.getColumn() + 1 + literalStart
                )
            );
        }

        return segments;
    }

    // Returns the source text represented by one token.
    string Parser::getLexeme(const lexer::Token& token) const {
        return string(token.getLexeme(sourceFile_));
    }

    // Converts a lexer token position into an AST source location.
    source::SourceLocation Parser::getLocation(
        const lexer::Token& token
    ) noexcept {
        return source::SourceLocation(token.getLine(), token.getColumn());
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
