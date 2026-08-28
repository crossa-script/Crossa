#pragma once

#include <cstddef>
#include <string_view>

#include "crossa/compiler/lexer/TokenType.h"
#include "crossa/compiler/source/SourceFile.h"

namespace crossa::compiler::lexer {

// Describes one lexical unit through its type and source location.
// Its accessors expose the token range without copying source text.
class Token final {
public:
    // Creates a token for one source range and its starting location.
    Token(
        TokenType type,
        std::size_t offset,
        std::size_t length,
        std::size_t line,
        std::size_t column
    ) noexcept;

    // Returns the token category.
    [[nodiscard]] TokenType getType() const noexcept;

    // Returns the byte offset where the token starts.
    [[nodiscard]] std::size_t getOffset() const noexcept;

    // Returns the token length in bytes.
    [[nodiscard]] std::size_t getLength() const noexcept;

    // Returns the one-based source line where the token starts.
    [[nodiscard]] std::size_t getLine() const noexcept;

    // Returns the one-based source column where the token starts.
    [[nodiscard]] std::size_t getColumn() const noexcept;

    // Returns a non-owning view of this token in the provided source file.
    [[nodiscard]] std::string_view getLexeme(
        const source::SourceFile& sourceFile
    ) const noexcept;

private:
    TokenType type_;
    std::size_t offset_;
    std::size_t length_;
    std::size_t line_;
    std::size_t column_;
};

}
