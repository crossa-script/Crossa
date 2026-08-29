#pragma once

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

#include "crossa/compiler/lexer/Token.h"
#include "crossa/compiler/lexer/TokenType.h"
#include "crossa/compiler/source/SourceFile.h"

namespace crossa::compiler::lexer {

// Tokenizes one loaded .cra source file according to CRA Language V0.
// tokenize() emits source-aware tokens for the future parser and diagnostics.
class Lexer final {
public:
    // Creates a lexer that reads from the provided source file.
    explicit Lexer(const source::SourceFile& sourceFile) noexcept;

    // Tokenizes the complete source and appends one EndOfFile token.
    [[nodiscard]] std::vector<Token> tokenize();

private:
    // Resets all mutable scanning state for a new tokenization pass.
    void reset();

    // Scans the next token from the current source position.
    void scanToken();

    // Scans an identifier and resolves reserved language words.
    void scanIdentifier();

    // Scans one integer or JSON decimal/exponent number literal.
    void scanNumber();

    // Scans one double-quoted string literal.
    void scanString();

    // Scans one hash-delimited import filename.
    void scanImportPath();

    // Scans and validates one execution annotation.
    void scanAnnotation();

    // Consumes the remainder of a single-line comment.
    void skipLineComment();

    // Appends a token for the current source range.
    void addToken(TokenType type);

    // Returns whether the scanner reached the end of the source.
    [[nodiscard]] bool isAtEnd() const noexcept;

    // Consumes and returns the current source byte.
    char advance() noexcept;

    // Consumes the expected byte when it appears next.
    [[nodiscard]] bool match(char expected) noexcept;

    // Returns the current byte without consuming it.
    [[nodiscard]] char peek() const noexcept;

    // Returns the byte after the current byte without consuming it.
    [[nodiscard]] char peekNext() const noexcept;

    // Resolves an identifier lexeme to its keyword or literal token type.
    [[nodiscard]] static TokenType resolveIdentifierType(
        std::string_view lexeme
    ) noexcept;

    // Returns whether a byte can begin an identifier.
    [[nodiscard]] static bool isIdentifierStart(char value) noexcept;

    // Returns whether a byte can continue an identifier.
    [[nodiscard]] static bool isIdentifierPart(char value) noexcept;

    // Returns whether a byte is an ASCII decimal digit.
    [[nodiscard]] static bool isDigit(char value) noexcept;

    // Throws a deterministic source-aware lexer failure.
    [[noreturn]] void fail(const std::string& message) const;

    const source::SourceFile& sourceFile_;
    std::vector<Token> tokens_;
    std::size_t startOffset_;
    std::size_t currentOffset_;
    std::size_t line_;
    std::size_t column_;
    std::size_t tokenLine_;
    std::size_t tokenColumn_;
};

}
