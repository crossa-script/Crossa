#include "crossa/compiler/lexer/Token.h"

using namespace std;

namespace crossa::compiler::lexer {

    // Creates a token for one source range and its starting location.
    Token::Token(
        TokenType type,
        size_t offset,
        size_t length,
        size_t line,
        size_t column
    ) noexcept
        : type_(type),
          offset_(offset),
          length_(length),
          line_(line),
          column_(column) {}

    // Returns the token category.
    TokenType Token::getType() const noexcept {
        return type_;
    }

    // Returns the byte offset where the token starts.
    size_t Token::getOffset() const noexcept {
        return offset_;
    }

    // Returns the token length in bytes.
    size_t Token::getLength() const noexcept {
        return length_;
    }

    // Returns the one-based source line where the token starts.
    size_t Token::getLine() const noexcept {
        return line_;
    }

    // Returns the one-based source column where the token starts.
    size_t Token::getColumn() const noexcept {
        return column_;
    }

    // Returns a non-owning view of this token in the provided source file.
    string_view Token::getLexeme(
        const source::SourceFile& sourceFile
    ) const noexcept {
        const string& content = sourceFile.getContent();
        if (offset_ > content.size() || length_ > content.size() - offset_) {
            return {};
        }

        return string_view(content).substr(offset_, length_);
    }

}
