#pragma once

#include <string_view>

namespace crossa::compiler::lexer {

// Identifies every lexical unit supported by CRA Language V0.
// Keyword and symbol names map directly to the documented language surface.
enum class TokenType {
    Identifier,
    IntegerLiteral,
    DecimalLiteral,
    StringLiteral,
    BooleanLiteral,
    ImportPath,

    KeywordImport,
    KeywordFun,
    KeywordRe,
    KeywordVar,
    KeywordModel,
    KeywordConfig,
    KeywordPrint,
    KeywordAssert,
    KeywordInt,
    KeywordString,
    KeywordBool,
    KeywordList,
    KeywordJson,
    KeywordNull,
    KeywordCrossaRequest,

    AnnotationSync,
    AnnotationAsync,
    AnnotationAsyncAfter,

    MethodGet,
    MethodPost,
    MethodPut,
    MethodPatch,
    MethodDelete,
    MethodHead,
    MethodOptions,
    MethodTrace,
    MethodConnect,

    LeftParen,
    RightParen,
    LeftBrace,
    RightBrace,
    LeftBracket,
    RightBracket,
    LeftAngle,
    RightAngle,
    Colon,
    Comma,
    Equal,
    Plus,
    Minus,
    Star,
    Slash,

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
