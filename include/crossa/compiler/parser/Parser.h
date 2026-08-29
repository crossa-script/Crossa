#pragma once

#include <cstddef>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "crossa/compiler/ast/Declaration.h"
#include "crossa/compiler/ast/CrossaRequestExpression.h"
#include "crossa/compiler/ast/Expression.h"
#include "crossa/compiler/ast/JsonExpression.h"
#include "crossa/compiler/ast/SourceUnit.h"
#include "crossa/compiler/ast/Statement.h"
#include "crossa/compiler/ast/TypeReference.h"
#include "crossa/compiler/lexer/Token.h"
#include "crossa/compiler/lexer/TokenType.h"
#include "crossa/compiler/source/SourceFile.h"
#include "crossa/compiler/source/SourceLocation.h"

namespace crossa::compiler::parser {

// Converts lexer tokens into a syntax-only Crossa AST.
// parse() owns declarations, JSON values, requests, and top-level calls.
class Parser final {
public:
    // Creates a parser over one token stream and its owning source file.
    Parser(
        const std::vector<lexer::Token>& tokens,
        const source::SourceFile& sourceFile
    );

    // Parses the complete token stream into one AST source unit.
    [[nodiscard]] ast::SourceUnit parse();

private:
    // Parses one top-level import after consuming the import keyword.
    [[nodiscard]] std::unique_ptr<ast::Declaration> parseImportDeclaration();

    // Parses one top-level declaration.
    [[nodiscard]] std::unique_ptr<ast::Declaration> parseDeclaration();

    // Parses a function preceded by one execution annotation.
    [[nodiscard]] std::unique_ptr<ast::Declaration> parseAnnotatedFunction(
        ast::ExecutionPolicy executionPolicy
    );

    // Parses a function declaration after consuming the fun keyword.
    [[nodiscard]] std::unique_ptr<ast::Declaration> parseFunctionDeclaration(
        ast::ExecutionPolicy executionPolicy
    );

    // Parses a top-level variable after consuming the var keyword.
    [[nodiscard]] std::unique_ptr<ast::Declaration>
    parseVariableDeclaration();

    // Parses a model declaration after consuming the model keyword.
    [[nodiscard]] std::unique_ptr<ast::Declaration> parseModelDeclaration();

    // Parses a config declaration after consuming the config keyword.
    [[nodiscard]] std::unique_ptr<ast::Declaration> parseConfigDeclaration();

    // Parses one function-body statement.
    [[nodiscard]] std::unique_ptr<ast::Statement> parseStatement();

    [[nodiscard]] std::unique_ptr<ast::Statement> parseIfStatement(
        source::SourceLocation location
    );

    [[nodiscard]] std::vector<std::unique_ptr<ast::Statement>>
    parseBlockStatements();

    // Parses a local variable after consuming the var keyword.
    [[nodiscard]] std::unique_ptr<ast::Statement> parseVariableStatement();

    // Parses one type reference including nested List<T> forms.
    [[nodiscard]] ast::TypeReference parseTypeReference();

    // Parses one expression using arithmetic precedence.
    [[nodiscard]] std::unique_ptr<ast::Expression> parseExpression();

    [[nodiscard]] std::unique_ptr<ast::Expression> parseLogicalOrExpression();

    [[nodiscard]] std::unique_ptr<ast::Expression> parseLogicalAndExpression();

    [[nodiscard]] std::unique_ptr<ast::Expression> parseEqualityExpression();

    [[nodiscard]] std::unique_ptr<ast::Expression> parseComparisonExpression();

    // Parses addition and subtraction expressions.
    [[nodiscard]] std::unique_ptr<ast::Expression> parseAdditiveExpression();

    // Parses multiplication and division expressions.
    [[nodiscard]] std::unique_ptr<ast::Expression>
    parseMultiplicativeExpression();

    // Parses unary negation expressions.
    [[nodiscard]] std::unique_ptr<ast::Expression> parseUnaryExpression();

    // Parses literals, identifiers, calls, and grouped expressions.
    [[nodiscard]] std::unique_ptr<ast::Expression> parsePrimaryExpression();

    // Parses a CrossaRequest builder after consuming its keyword.
    [[nodiscard]] std::unique_ptr<ast::Expression>
    parseCrossaRequestExpression(source::SourceLocation location);

    // Parses a JSON object after consuming its left brace.
    [[nodiscard]] std::unique_ptr<ast::Expression>
    parseJsonObjectExpression(source::SourceLocation location);

    // Parses a JSON array after consuming its left bracket.
    [[nodiscard]] std::unique_ptr<ast::Expression>
    parseJsonArrayExpression(source::SourceLocation location);

    // Parses one standard HTTP method literal.
    [[nodiscard]] std::unique_ptr<ast::Expression>
    parseHttpMethodExpression();

    // Parses an identifier or static string JSON object key.
    [[nodiscard]] std::string parseJsonObjectKey();

    // Parses call arguments after consuming the left parenthesis.
    [[nodiscard]] std::unique_ptr<ast::Expression> parseCallExpression(
        std::string callee,
        source::SourceLocation location
    );

    // Parses literal and identifier segments from one string token.
    [[nodiscard]] std::vector<ast::StringSegment> parseStringSegments(
        const lexer::Token& token
    ) const;

    // Decodes one source escape and advances past its complete byte sequence.
    void appendStringEscape(
        std::string_view content,
        std::size_t& current,
        std::string& output,
        const lexer::Token& token
    ) const;

    // Reads one four-digit Unicode code unit from source string content.
    [[nodiscard]] unsigned int readHexCodeUnit(
        std::string_view content,
        std::size_t& current,
        const lexer::Token& token
    ) const;

    // Appends one Unicode code point to a UTF-8 string.
    static void appendCodePoint(unsigned int codePoint, std::string& output);

    // Returns the source text represented by one token.
    [[nodiscard]] std::string getLexeme(const lexer::Token& token) const;

    // Converts a lexer token position into an AST source location.
    [[nodiscard]] source::SourceLocation getLocation(
        const lexer::Token& token
    ) const noexcept;

    // Creates a source location inside the parser's current file.
    [[nodiscard]] source::SourceLocation getLocation(
        std::size_t line,
        std::size_t column
    ) const noexcept;

    // Consumes one token when its type matches the expectation.
    const lexer::Token& consume(
        lexer::TokenType type,
        const std::string& message
    );

    // Consumes and reports whether the next token has the requested type.
    [[nodiscard]] bool match(lexer::TokenType type) noexcept;

    // Returns whether the current token has the requested type.
    [[nodiscard]] bool check(lexer::TokenType type) const noexcept;

    // Consumes and returns the current token.
    const lexer::Token& advance() noexcept;

    // Returns whether the parser reached the EndOfFile token.
    [[nodiscard]] bool isAtEnd() const noexcept;

    // Returns the current token without consuming it.
    [[nodiscard]] const lexer::Token& peek() const noexcept;

    // Returns the most recently consumed token.
    [[nodiscard]] const lexer::Token& previous() const noexcept;

    // Returns whether a byte can begin an interpolation identifier.
    [[nodiscard]] static bool isIdentifierStart(char value) noexcept;

    // Returns whether a byte can continue an interpolation identifier.
    [[nodiscard]] static bool isIdentifierPart(char value) noexcept;

    // Throws a deterministic parser failure at the provided token.
    [[noreturn]] void fail(
        const lexer::Token& token,
        const std::string& message
    ) const;

    const std::vector<lexer::Token>& tokens_;
    const source::SourceFile& sourceFile_;
    std::shared_ptr<const std::string> sourcePath_;
    std::size_t current_;
};

}
