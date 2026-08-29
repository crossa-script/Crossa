#include "crossa/compiler/generators/kotlin/KotlinIdentifierEscaper.h"

#include <array>
#include <string_view>

using namespace std;

namespace crossa::compiler::generators::kotlin {

    // Returns a Kotlin declaration or reference identifier for one Crossa name.
    string KotlinIdentifierEscaper::escape(const string& identifier) {
        return isKeyword(identifier) ? "`" + identifier + "`" : identifier;
    }

    // Returns whether one Crossa identifier is reserved by Kotlin.
    bool KotlinIdentifierEscaper::isKeyword(
        const string& identifier
    ) noexcept {
        static constexpr array<string_view, 77> Keywords = {
            "actual", "abstract", "annotation", "as", "break", "by",
            "catch", "class", "companion", "const", "constructor",
            "continue", "crossinline", "data", "delegate", "do", "dynamic",
            "else", "enum", "expect", "external", "false", "field", "file",
            "final", "finally", "for", "fun", "get", "if", "import", "in",
            "infix", "init", "inline", "inner", "interface", "internal", "is",
            "lateinit", "noinline", "null", "object", "open", "operator", "out",
            "override", "package", "param", "private", "property", "protected",
            "public", "receiver", "reified", "return", "sealed", "set",
            "setparam", "super", "suspend", "tailrec", "this", "throw", "true",
            "try", "typealias", "val", "var", "vararg", "when", "where",
            "while", "yield"
        };
        for (const string_view keyword : Keywords) {
            if (identifier == keyword) {
                return true;
            }
        }
        return false;
    }

}
