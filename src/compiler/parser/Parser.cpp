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

        fail(peek(), "Expected a top-level declaration.");
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
                    parseTypeReference()
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
            std::move(statements)
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
            std::move(initializer)
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
                fields.emplace_back(getLexeme(fieldName), parseTypeReference());
            } while (match(lexer::TokenType::Comma));
        }

        consume(lexer::TokenType::RightParen, "Expected ')' after model fields.");
        return make_unique<ast::ModelDeclaration>(
            getLexeme(nameToken),
            std::move(fields)
        );
    }

    // Parses a config declaration after consuming the config keyword.
    unique_ptr<ast::Declaration> Parser::parseConfigDeclaration() {
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
                entries.emplace_back(getLexeme(entryName), parseExpression());
            } while (match(lexer::TokenType::Comma));
        }

        consume(lexer::TokenType::RightBrace, "Expected '}' after config entries.");
        return make_unique<ast::ConfigDeclaration>(std::move(entries));
    }

    // Parses one function-body statement.
    unique_ptr<ast::Statement> Parser::parseStatement() {
        if (match(lexer::TokenType::KeywordRe)) {
            return make_unique<ast::ReturnStatement>(parseExpression());
        }
        if (match(lexer::TokenType::KeywordVar)) {
            return parseVariableStatement();
        }

        return make_unique<ast::ExpressionStatement>(parseExpression());
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
            parseExpression()
        );
    }

    // Parses one type reference including nested List<T> forms.
    ast::TypeReference Parser::parseTypeReference() {
        if (match(lexer::TokenType::KeywordList)) {
            consume(lexer::TokenType::LeftAngle, "Expected '<' after 'List'.");
            ast::TypeReference elementType = parseTypeReference();
            consume(lexer::TokenType::RightAngle, "Expected '>' after the List type.");
            return ast::TypeReference::createList(std::move(elementType));
        }

        if (match(lexer::TokenType::KeywordInt) ||
            match(lexer::TokenType::KeywordString) ||
            match(lexer::TokenType::KeywordBool) ||
            match(lexer::TokenType::Identifier)) {
            return ast::TypeReference::createNamed(getLexeme(previous()));
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
            const lexer::TokenType operation = advance().getType();
            const ast::BinaryOperator binaryOperator =
                operation == lexer::TokenType::Plus
                    ? ast::BinaryOperator::Add
                    : ast::BinaryOperator::Subtract;
            expression = make_unique<ast::BinaryExpression>(
                std::move(expression),
                binaryOperator,
                parseMultiplicativeExpression()
            );
        }

        return expression;
    }

    // Parses multiplication and division expressions.
    unique_ptr<ast::Expression> Parser::parseMultiplicativeExpression() {
        unique_ptr<ast::Expression> expression = parseUnaryExpression();

        while (check(lexer::TokenType::Star) ||
               check(lexer::TokenType::Slash)) {
            const lexer::TokenType operation = advance().getType();
            const ast::BinaryOperator binaryOperator =
                operation == lexer::TokenType::Star
                    ? ast::BinaryOperator::Multiply
                    : ast::BinaryOperator::Divide;
            expression = make_unique<ast::BinaryExpression>(
                std::move(expression),
                binaryOperator,
                parseUnaryExpression()
            );
        }

        return expression;
    }

    // Parses unary negation expressions.
    unique_ptr<ast::Expression> Parser::parseUnaryExpression() {
        if (match(lexer::TokenType::Minus)) {
            return make_unique<ast::UnaryExpression>(
                ast::UnaryOperator::Negate,
                parseUnaryExpression()
            );
        }

        return parsePrimaryExpression();
    }

    // Parses literals, identifiers, calls, and grouped expressions.
    unique_ptr<ast::Expression> Parser::parsePrimaryExpression() {
        if (match(lexer::TokenType::IntegerLiteral)) {
            return make_unique<ast::IntegerLiteralExpression>(getLexeme(previous()));
        }

        if (match(lexer::TokenType::StringLiteral)) {
            return make_unique<ast::StringLiteralExpression>(
                parseStringSegments(previous())
            );
        }

        if (match(lexer::TokenType::BooleanLiteral)) {
            return make_unique<ast::BooleanLiteralExpression>(
                getLexeme(previous()) == "true"
            );
        }

        if (match(lexer::TokenType::KeywordCrossaRequest)) {
            fail(
                previous(),
                "CrossaRequest parsing is not implemented in this milestone."
            );
        }

        if (match(lexer::TokenType::KeywordPrint)) {
            consume(lexer::TokenType::LeftParen, "Expected '(' after 'print'.");
            return parseCallExpression("print");
        }

        if (match(lexer::TokenType::Identifier)) {
            const string name = getLexeme(previous());
            if (match(lexer::TokenType::LeftParen)) {
                return parseCallExpression(name);
            }
            return make_unique<ast::IdentifierExpression>(name);
        }

        if (match(lexer::TokenType::LeftParen)) {
            unique_ptr<ast::Expression> expression = parseExpression();
            consume(lexer::TokenType::RightParen, "Expected ')' after the expression.");
            return expression;
        }

        fail(peek(), "Expected an expression.");
    }

    // Parses call arguments after consuming the left parenthesis.
    unique_ptr<ast::Expression> Parser::parseCallExpression(string callee) {
        vector<unique_ptr<ast::Expression>> arguments;
        if (!check(lexer::TokenType::RightParen)) {
            do {
                arguments.push_back(parseExpression());
            } while (match(lexer::TokenType::Comma));
        }

        consume(lexer::TokenType::RightParen, "Expected ')' after call arguments.");
        return make_unique<ast::CallExpression>(
            std::move(callee),
            std::move(arguments)
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
                    string(content.substr(literalStart, current - literalStart))
                );
            }

            const size_t identifierStart = ++current;
            while (current < content.size() &&
                   isIdentifierPart(content[current])) {
                ++current;
            }
            segments.emplace_back(
                ast::StringSegmentKind::Identifier,
                string(content.substr(identifierStart, current - identifierStart))
            );
            literalStart = current;
        }

        if (literalStart < content.size()) {
            segments.emplace_back(
                ast::StringSegmentKind::Literal,
                string(content.substr(literalStart))
            );
        }

        return segments;
    }

    // Returns the source text represented by one token.
    string Parser::getLexeme(const lexer::Token& token) const {
        return string(token.getLexeme(sourceFile_));
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
