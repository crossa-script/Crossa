#include "crossa/compiler/lexer/Lexer.h"

#include <stdexcept>
#include <utility>

using namespace std;

namespace crossa::compiler::lexer {

    // Creates a lexer that reads from the provided source file.
    Lexer::Lexer(const source::SourceFile& sourceFile) noexcept
        : sourceFile_(sourceFile),
          startOffset_(0),
          currentOffset_(0),
          line_(1),
          column_(1),
          tokenLine_(1),
          tokenColumn_(1) {}

    // Tokenizes the complete source and appends one EndOfFile token.
    vector<Token> Lexer::tokenize() {
        reset();

        while (!isAtEnd()) {
            startOffset_ = currentOffset_;
            tokenLine_ = line_;
            tokenColumn_ = column_;
            scanToken();
        }

        tokens_.emplace_back(
            TokenType::EndOfFile,
            currentOffset_,
            0,
            line_,
            column_
        );

        return std::move(tokens_);
    }

    // Resets all mutable scanning state for a new tokenization pass.
    void Lexer::reset() {
        tokens_.clear();
        startOffset_ = 0;
        currentOffset_ = 0;
        line_ = 1;
        column_ = 1;
        tokenLine_ = 1;
        tokenColumn_ = 1;
    }

    // Scans the next token from the current source position.
    void Lexer::scanToken() {
        const char value = advance();

        switch (value) {
            case '(':
                addToken(TokenType::LeftParen);
                return;
            case ')':
                addToken(TokenType::RightParen);
                return;
            case '{':
                addToken(TokenType::LeftBrace);
                return;
            case '}':
                addToken(TokenType::RightBrace);
                return;
            case '[':
                addToken(TokenType::LeftBracket);
                return;
            case ']':
                addToken(TokenType::RightBracket);
                return;
            case '<':
                addToken(match('=') ? TokenType::LessEqual : TokenType::LeftAngle);
                return;
            case '>':
                addToken(match('=') ? TokenType::GreaterEqual : TokenType::RightAngle);
                return;
            case ':':
                addToken(TokenType::Colon);
                return;
            case ',':
                addToken(TokenType::Comma);
                return;
            case '=':
                addToken(match('=') ? TokenType::EqualEqual : TokenType::Equal);
                return;
            case '!':
                addToken(match('=') ? TokenType::BangEqual : TokenType::Bang);
                return;
            case '&':
                if (!match('&')) {
                    fail("Expected '&' after '&'.");
                }
                addToken(TokenType::AndAnd);
                return;
            case '|':
                if (!match('|')) {
                    fail("Expected '|' after '|'.");
                }
                addToken(TokenType::OrOr);
                return;
            case '+':
                addToken(TokenType::Plus);
                return;
            case '-':
                addToken(TokenType::Minus);
                return;
            case '*':
                addToken(TokenType::Star);
                return;
            case '/':
                if (match('/')) {
                    skipLineComment();
                } else {
                    addToken(TokenType::Slash);
                }
                return;
            case '#':
                scanImportPath();
                return;
            case '@':
                scanAnnotation();
                return;
            case '"':
                scanString();
                return;
            case ' ':
            case '\r':
            case '\t':
            case '\n':
                return;
            default:
                break;
        }

        if (isDigit(value)) {
            scanNumber();
            return;
        }

        if (isIdentifierStart(value)) {
            scanIdentifier();
            return;
        }

        fail("Unexpected character '" + string(1, value) + "'.");
    }

    // Scans an identifier and resolves reserved language words.
    void Lexer::scanIdentifier() {
        while (isIdentifierPart(peek())) {
            advance();
        }

        const string& content = sourceFile_.getContent();
        const string_view lexeme(content.data() + startOffset_, currentOffset_ - startOffset_);
        addToken(resolveIdentifierType(lexeme));
    }

    // Scans one integer or JSON decimal/exponent number literal.
    void Lexer::scanNumber() {
        while (isDigit(peek())) {
            advance();
        }

        bool decimal = false;
        if (peek() == '.' && isDigit(peekNext())) {
            decimal = true;
            advance();
            while (isDigit(peek())) {
                advance();
            }
        }
        if (peek() == 'e' || peek() == 'E') {
            decimal = true;
            advance();
            if (peek() == '+' || peek() == '-') {
                advance();
            }
            if (!isDigit(peek())) {
                fail("Expected digits after the JSON number exponent.");
            }
            while (isDigit(peek())) {
                advance();
            }
        }

        addToken(decimal ? TokenType::DecimalLiteral : TokenType::IntegerLiteral);
    }

    // Scans one double-quoted string literal.
    void Lexer::scanString() {
        bool escaped = false;
        while (!isAtEnd()) {
            if (!escaped && peek() == '"') {
                break;
            }
            if (peek() == '\n' || peek() == '\r') {
                fail("Unterminated string literal.");
            }
            if (escaped) {
                escaped = false;
            } else if (peek() == '\\') {
                escaped = true;
            }
            advance();
        }

        if (isAtEnd()) {
            fail("Unterminated string literal.");
        }

        advance();
        addToken(TokenType::StringLiteral);
    }

    // Scans one hash-delimited import filename.
    void Lexer::scanImportPath() {
        while (!isAtEnd() && peek() != '#') {
            if (peek() == '\n' || peek() == '\r') {
                fail("Import filename must end with '#'.");
            }
            if (peek() == ' ' || peek() == '\t') {
                fail("Import filename cannot contain whitespace.");
            }
            advance();
        }

        if (isAtEnd()) {
            fail("Import filename must end with '#'.");
        }

        advance();
        addToken(TokenType::ImportPath);
    }

    // Scans and validates one execution annotation.
    void Lexer::scanAnnotation() {
        if (!isIdentifierStart(peek())) {
            fail("Expected an execution annotation after '@'.");
        }

        while (isIdentifierPart(peek())) {
            advance();
        }

        const string& content = sourceFile_.getContent();
        const string_view lexeme(content.data() + startOffset_, currentOffset_ - startOffset_);

        if (lexeme == "@Sync") {
            addToken(TokenType::AnnotationSync);
            return;
        }

        if (lexeme == "@Async") {
            addToken(TokenType::AnnotationAsync);
            return;
        }

        if (lexeme == "@AsyncAfter") {
            addToken(TokenType::AnnotationAsyncAfter);
            return;
        }

        fail("Unsupported execution annotation '" + string(lexeme) + "'.");
    }

    // Consumes the remainder of a single-line comment.
    void Lexer::skipLineComment() {
        while (!isAtEnd() && peek() != '\n') {
            advance();
        }
    }

    // Appends a token for the current source range.
    void Lexer::addToken(TokenType type) {
        tokens_.emplace_back(
            type,
            startOffset_,
            currentOffset_ - startOffset_,
            tokenLine_,
            tokenColumn_
        );
    }

    // Returns whether the scanner reached the end of the source.
    bool Lexer::isAtEnd() const noexcept {
        return currentOffset_ >= sourceFile_.getContent().size();
    }

    // Consumes and returns the current source byte.
    char Lexer::advance() noexcept {
        if (isAtEnd()) {
            return '\0';
        }

        const char value = sourceFile_.getContent()[currentOffset_++];
        if (value == '\n') {
            ++line_;
            column_ = 1;
        } else {
            ++column_;
        }

        return value;
    }

    // Consumes the expected byte when it appears next.
    bool Lexer::match(char expected) noexcept {
        if (isAtEnd() || peek() != expected) {
            return false;
        }

        advance();
        return true;
    }

    // Returns the current byte without consuming it.
    char Lexer::peek() const noexcept {
        if (isAtEnd()) {
            return '\0';
        }

        return sourceFile_.getContent()[currentOffset_];
    }

    // Returns the byte after the current byte without consuming it.
    char Lexer::peekNext() const noexcept {
        const size_t nextOffset = currentOffset_ + 1;
        if (nextOffset >= sourceFile_.getContent().size()) {
            return '\0';
        }
        return sourceFile_.getContent()[nextOffset];
    }

    // Resolves an identifier lexeme to its keyword or literal token type.
    TokenType Lexer::resolveIdentifierType(string_view lexeme) noexcept {
        if (lexeme == "import") {
            return TokenType::KeywordImport;
        }
        if (lexeme == "fun") {
            return TokenType::KeywordFun;
        }
        if (lexeme == "re") {
            return TokenType::KeywordRe;
        }
        if (lexeme == "if") {
            return TokenType::KeywordIf;
        }
        if (lexeme == "else") {
            return TokenType::KeywordElse;
        }
        if (lexeme == "var") {
            return TokenType::KeywordVar;
        }
        if (lexeme == "model") {
            return TokenType::KeywordModel;
        }
        if (lexeme == "config") {
            return TokenType::KeywordConfig;
        }
        if (lexeme == "print") {
            return TokenType::KeywordPrint;
        }
        if (lexeme == "assert") {
            return TokenType::KeywordAssert;
        }
        if (lexeme == "Int") {
            return TokenType::KeywordInt;
        }
        if (lexeme == "Long") {
            return TokenType::KeywordLong;
        }
        if (lexeme == "Double") {
            return TokenType::KeywordDouble;
        }
        if (lexeme == "String") {
            return TokenType::KeywordString;
        }
        if (lexeme == "Bool") {
            return TokenType::KeywordBool;
        }
        if (lexeme == "List") {
            return TokenType::KeywordList;
        }
        if (lexeme == "Json") {
            return TokenType::KeywordJson;
        }
        if (lexeme == "null") {
            return TokenType::KeywordNull;
        }
        if (lexeme == "CrossaRequest") {
            return TokenType::KeywordCrossaRequest;
        }
        if (lexeme == "GET") {
            return TokenType::MethodGet;
        }
        if (lexeme == "POST") {
            return TokenType::MethodPost;
        }
        if (lexeme == "PUT") {
            return TokenType::MethodPut;
        }
        if (lexeme == "PATCH") {
            return TokenType::MethodPatch;
        }
        if (lexeme == "DELETE") {
            return TokenType::MethodDelete;
        }
        if (lexeme == "HEAD") {
            return TokenType::MethodHead;
        }
        if (lexeme == "OPTIONS") {
            return TokenType::MethodOptions;
        }
        if (lexeme == "TRACE") {
            return TokenType::MethodTrace;
        }
        if (lexeme == "CONNECT") {
            return TokenType::MethodConnect;
        }
        if (lexeme == "true" || lexeme == "false") {
            return TokenType::BooleanLiteral;
        }

        return TokenType::Identifier;
    }

    // Returns whether a byte can begin an identifier.
    bool Lexer::isIdentifierStart(char value) noexcept {
        return (value >= 'a' && value <= 'z') ||
               (value >= 'A' && value <= 'Z') ||
               value == '_';
    }

    // Returns whether a byte can continue an identifier.
    bool Lexer::isIdentifierPart(char value) noexcept {
        return isIdentifierStart(value) || isDigit(value);
    }

    // Returns whether a byte is an ASCII decimal digit.
    bool Lexer::isDigit(char value) noexcept {
        return value >= '0' && value <= '9';
    }

    // Throws a deterministic source-aware lexer failure.
    [[noreturn]] void Lexer::fail(const string& message) const {
        throw runtime_error(
            sourceFile_.getPath().string() + ":" +
            to_string(tokenLine_) + ":" +
            to_string(tokenColumn_) + ": " +
            message
        );
    }

}
