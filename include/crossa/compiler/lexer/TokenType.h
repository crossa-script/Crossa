#pragma once

#include <string_view>

namespace crossa::compiler::lexer {

// Identifies every lexical unit supported by CRA Language V0.
// Keyword and symbol names map directly to the documented language surface.
enum class TokenType {
    Identifier,
    IntegerLiteral,
    StringLiteral,
    BooleanLiteral,

    KeywordFun,
    KeywordRe,
    KeywordVar,
    KeywordModel,
    KeywordConfig,
    KeywordPrint,
    KeywordInt,
    KeywordString,
    KeywordBool,
    KeywordList,
    KeywordCrossaRequest,

    AnnotationSync,
    AnnotationAsync,
    AnnotationAsyncAfter,

    MethodGet,

    LeftParen,
    RightParen,
    LeftBrace,
    RightBrace,
    LeftAngle,
    RightAngle,
    Colon,
    Comma,
    Equal,
    Plus,
    Minus,
    Star,
    Slash,
    Hash,

    EndOfFile
};

// Converts token enum values into stable readable names.
// toString() is primarily used by diagnostics and debug output.
class TokenTypeUtils final {
public:
    // Returns the stable readable name of a token type.
    [[nodiscard]] static std::string_view toString(TokenType type) noexcept;
};

}
