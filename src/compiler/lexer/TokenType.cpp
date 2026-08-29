#include "crossa/compiler/lexer/TokenType.h"

using namespace std;

namespace crossa::compiler::lexer {

    // Returns the stable readable name of a token type.
    string_view TokenTypeUtils::toString(TokenType type) noexcept {
        switch (type) {
            case TokenType::Identifier:
                return "Identifier";
            case TokenType::IntegerLiteral:
                return "IntegerLiteral";
            case TokenType::DecimalLiteral:
                return "DecimalLiteral";
            case TokenType::StringLiteral:
                return "StringLiteral";
            case TokenType::BooleanLiteral:
                return "BooleanLiteral";
            case TokenType::KeywordFun:
                return "KeywordFun";
            case TokenType::KeywordRe:
                return "KeywordRe";
            case TokenType::KeywordVar:
                return "KeywordVar";
            case TokenType::KeywordModel:
                return "KeywordModel";
            case TokenType::KeywordConfig:
                return "KeywordConfig";
            case TokenType::KeywordPrint:
                return "KeywordPrint";
            case TokenType::KeywordInt:
                return "KeywordInt";
            case TokenType::KeywordString:
                return "KeywordString";
            case TokenType::KeywordBool:
                return "KeywordBool";
            case TokenType::KeywordList:
                return "KeywordList";
            case TokenType::KeywordJson:
                return "KeywordJson";
            case TokenType::KeywordNull:
                return "KeywordNull";
            case TokenType::KeywordCrossaRequest:
                return "KeywordCrossaRequest";
            case TokenType::AnnotationSync:
                return "AnnotationSync";
            case TokenType::AnnotationAsync:
                return "AnnotationAsync";
            case TokenType::AnnotationAsyncAfter:
                return "AnnotationAsyncAfter";
            case TokenType::MethodGet:
                return "MethodGet";
            case TokenType::MethodPost:
                return "MethodPost";
            case TokenType::MethodPut:
                return "MethodPut";
            case TokenType::MethodPatch:
                return "MethodPatch";
            case TokenType::MethodDelete:
                return "MethodDelete";
            case TokenType::MethodHead:
                return "MethodHead";
            case TokenType::MethodOptions:
                return "MethodOptions";
            case TokenType::MethodTrace:
                return "MethodTrace";
            case TokenType::MethodConnect:
                return "MethodConnect";
            case TokenType::LeftParen:
                return "LeftParen";
            case TokenType::RightParen:
                return "RightParen";
            case TokenType::LeftBrace:
                return "LeftBrace";
            case TokenType::RightBrace:
                return "RightBrace";
            case TokenType::LeftBracket:
                return "LeftBracket";
            case TokenType::RightBracket:
                return "RightBracket";
            case TokenType::LeftAngle:
                return "LeftAngle";
            case TokenType::RightAngle:
                return "RightAngle";
            case TokenType::Colon:
                return "Colon";
            case TokenType::Comma:
                return "Comma";
            case TokenType::Equal:
                return "Equal";
            case TokenType::Plus:
                return "Plus";
            case TokenType::Minus:
                return "Minus";
            case TokenType::Star:
                return "Star";
            case TokenType::Slash:
                return "Slash";
            case TokenType::Hash:
                return "Hash";
            case TokenType::EndOfFile:
                return "EndOfFile";
        }

        return "Unknown";
    }

}
