#include "crossa/compiler/generators/kotlin/KotlinExpressionEmitter.h"

#include <sstream>
#include <stdexcept>

#include "crossa/compiler/ir/IrJsonExpression.h"

using namespace std;

namespace crossa::compiler::generators::kotlin {

    // Creates an emitter using the shared Kotlin identifier escaping rules.
    KotlinExpressionEmitter::KotlinExpressionEmitter(
        const KotlinIdentifierEscaper& identifierEscaper
    ) noexcept
        : identifierEscaper_(identifierEscaper) {}

    // Emits one complete Kotlin expression from typed Crossa IR.
    string KotlinExpressionEmitter::emit(
        const ir::IrExpression& expression
    ) const {
        return emitWithParentPrecedence(expression, 0);
    }

    // Emits one expression and parenthesizes it when the parent requires it.
    string KotlinExpressionEmitter::emitWithParentPrecedence(
        const ir::IrExpression& expression,
        int parentPrecedence
    ) const {
        const string emitted = emitExpression(expression);
        return precedenceOf(expression) <= parentPrecedence
            ? "(" + emitted + ")"
            : emitted;
    }

    // Emits one direct Kotlin expression without parent-required parentheses.
    string KotlinExpressionEmitter::emitExpression(
        const ir::IrExpression& expression
    ) const {
        switch (expression.getKind()) {
            case ir::IrExpressionKind::ReadSymbol:
                return identifierEscaper_.escape(
                    static_cast<const ir::IrReadSymbolExpression&>(expression)
                        .getName()
                );
            case ir::IrExpressionKind::IntegerConstant:
                return static_cast<const ir::IrIntegerConstantExpression&>(
                    expression
                ).getValue();
            case ir::IrExpressionKind::DoubleConstant:
                return static_cast<const ir::IrDoubleConstantExpression&>(
                    expression
                ).getValue();
            case ir::IrExpressionKind::StringBuild:
                return emitStringBuild(
                    static_cast<const ir::IrStringBuildExpression&>(expression)
                );
            case ir::IrExpressionKind::BooleanConstant:
                return static_cast<const ir::IrBooleanConstantExpression&>(
                    expression
                ).getValue() ? "true" : "false";
            case ir::IrExpressionKind::Call:
                return emitCall(
                    static_cast<const ir::IrCallExpression&>(expression)
                );
            case ir::IrExpressionKind::Unary: {
                const auto& unary = static_cast<const ir::IrUnaryExpression&>(
                    expression
                );
                return string(mapUnaryOperator(unary.getOperator())) +
                    emitWithParentPrecedence(
                        unary.getOperand(),
                        precedenceOf(expression)
                    );
            }
            case ir::IrExpressionKind::Binary: {
                const auto& binary = static_cast<const ir::IrBinaryExpression&>(
                    expression
                );
                const int precedence = precedenceOf(binary.getOperator());
                return emitWithParentPrecedence(binary.getLeft(), precedence) +
                    " " + mapBinaryOperator(binary.getOperator()) + " " +
                    emitWithParentPrecedence(binary.getRight(), precedence);
            }
            case ir::IrExpressionKind::CrossaRequest:
                failUnsupported("runtime-backed expression 'CrossaRequest'");
            case ir::IrExpressionKind::JsonNumber:
            case ir::IrExpressionKind::JsonNull:
            case ir::IrExpressionKind::JsonObject:
            case ir::IrExpressionKind::JsonArray:
                failUnsupported("JSON expression");
        }

        failInvariant("Unknown IR expression kind.");
    }

    // Emits one resolved Kotlin function or supported builtin call.
    string KotlinExpressionEmitter::emitCall(
        const ir::IrCallExpression& expression
    ) const {
        if (expression.isBuiltin() && expression.getCallee() != "print") {
            failUnsupported("builtin call");
        }

        if (expression.isBuiltin() && expression.getArguments().size() != 1) {
            failInvariant("print builtin call has an invalid argument count.");
        }

        ostringstream output;
        output << (expression.isBuiltin()
                       ? "println"
                       : identifierEscaper_.escape(expression.getCallee()))
               << "(";
        for (size_t index = 0; index < expression.getArguments().size(); ++index) {
            if (index > 0) {
                output << ", ";
            }
            output << emit(*expression.getArguments()[index]);
        }
        output << ")";
        return output.str();
    }

    // Emits one pre-resolved string construction expression.
    string KotlinExpressionEmitter::emitStringBuild(
        const ir::IrStringBuildExpression& expression
    ) const {
        const vector<ir::IrStringSegment>& segments = expression.getSegments();
        if (segments.empty()) {
            return "\"\"";
        }

        ostringstream output;
        bool hasValue = false;
        if (segments.front().getKind() == ir::IrStringSegmentKind::Symbol) {
            output << "\"\"";
            hasValue = true;
        }
        for (const ir::IrStringSegment& segment : segments) {
            if (hasValue) {
                output << " + ";
            }
            if (segment.getKind() == ir::IrStringSegmentKind::Literal) {
                output << escapeStringLiteral(segment.getValue());
                hasValue = true;
                continue;
            }
            if (segment.getSymbolKind() == nullptr) {
                failInvariant("String symbol segment has no resolved owner.");
            }
            output << identifierEscaper_.escape(segment.getValue());
            hasValue = true;
        }
        return output.str();
    }

    // Escapes one Crossa string segment as a Kotlin string literal.
    string KotlinExpressionEmitter::escapeStringLiteral(const string& value) {
        ostringstream output;
        output << '"';
        for (const unsigned char character : value) {
            switch (character) {
                case '\\':
                    output << "\\\\";
                    break;
                case '"':
                    output << "\\\"";
                    break;
                case '$':
                    output << "\\$";
                    break;
                case '\n':
                    output << "\\n";
                    break;
                case '\r':
                    output << "\\r";
                    break;
                case '\t':
                    output << "\\t";
                    break;
                case '\b':
                    output << "\\b";
                    break;
                case '\f':
                    output << "\\f";
                    break;
                default:
                    if (character < 0x20) {
                        constexpr char HexDigits[] = "0123456789ABCDEF";
                        output << "\\u00" << HexDigits[character >> 4]
                               << HexDigits[character & 0x0F];
                    } else {
                        output << static_cast<char>(character);
                    }
                    break;
            }
        }
        output << '"';
        return output.str();
    }

    // Returns the target precedence for one typed IR expression.
    int KotlinExpressionEmitter::precedenceOf(
        const ir::IrExpression& expression
    ) {
        switch (expression.getKind()) {
            case ir::IrExpressionKind::Unary:
                return 80;
            case ir::IrExpressionKind::Binary:
                return precedenceOf(
                    static_cast<const ir::IrBinaryExpression&>(expression)
                        .getOperator()
                );
            case ir::IrExpressionKind::ReadSymbol:
            case ir::IrExpressionKind::IntegerConstant:
            case ir::IrExpressionKind::DoubleConstant:
            case ir::IrExpressionKind::StringBuild:
            case ir::IrExpressionKind::BooleanConstant:
            case ir::IrExpressionKind::Call:
            case ir::IrExpressionKind::JsonNumber:
            case ir::IrExpressionKind::JsonNull:
            case ir::IrExpressionKind::JsonObject:
            case ir::IrExpressionKind::JsonArray:
            case ir::IrExpressionKind::CrossaRequest:
                return 100;
        }

        failInvariant("Unknown IR expression precedence.");
    }

    // Returns the target precedence for one binary IR operation.
    int KotlinExpressionEmitter::precedenceOf(
        ir::IrArithmeticOperator operation
    ) {
        switch (operation) {
            case ir::IrArithmeticOperator::Multiply:
            case ir::IrArithmeticOperator::Divide:
                return 70;
            case ir::IrArithmeticOperator::Add:
            case ir::IrArithmeticOperator::Subtract:
                return 60;
            case ir::IrArithmeticOperator::Less:
            case ir::IrArithmeticOperator::LessEqual:
            case ir::IrArithmeticOperator::Greater:
            case ir::IrArithmeticOperator::GreaterEqual:
                return 50;
            case ir::IrArithmeticOperator::Equal:
            case ir::IrArithmeticOperator::NotEqual:
                return 40;
            case ir::IrArithmeticOperator::LogicalAnd:
                return 30;
            case ir::IrArithmeticOperator::LogicalOr:
                return 20;
            case ir::IrArithmeticOperator::Negate:
            case ir::IrArithmeticOperator::Not:
                failInvariant("Unary operation reached binary expression.");
        }

        failInvariant("Unknown binary operation.");
    }

    // Returns Kotlin syntax for one supported unary IR operation.
    const char* KotlinExpressionEmitter::mapUnaryOperator(
        ir::IrArithmeticOperator operation
    ) {
        switch (operation) {
            case ir::IrArithmeticOperator::Negate:
                return "-";
            case ir::IrArithmeticOperator::Not:
                return "!";
            case ir::IrArithmeticOperator::Add:
            case ir::IrArithmeticOperator::Subtract:
            case ir::IrArithmeticOperator::Multiply:
            case ir::IrArithmeticOperator::Divide:
            case ir::IrArithmeticOperator::Equal:
            case ir::IrArithmeticOperator::NotEqual:
            case ir::IrArithmeticOperator::Less:
            case ir::IrArithmeticOperator::LessEqual:
            case ir::IrArithmeticOperator::Greater:
            case ir::IrArithmeticOperator::GreaterEqual:
            case ir::IrArithmeticOperator::LogicalAnd:
            case ir::IrArithmeticOperator::LogicalOr:
                failInvariant("Binary operation reached unary expression.");
        }

        failInvariant("Unknown unary operation.");
    }

    // Returns Kotlin syntax for one supported binary IR operation.
    const char* KotlinExpressionEmitter::mapBinaryOperator(
        ir::IrArithmeticOperator operation
    ) {
        switch (operation) {
            case ir::IrArithmeticOperator::Add:
                return "+";
            case ir::IrArithmeticOperator::Subtract:
                return "-";
            case ir::IrArithmeticOperator::Multiply:
                return "*";
            case ir::IrArithmeticOperator::Divide:
                return "/";
            case ir::IrArithmeticOperator::Equal:
                return "==";
            case ir::IrArithmeticOperator::NotEqual:
                return "!=";
            case ir::IrArithmeticOperator::Less:
                return "<";
            case ir::IrArithmeticOperator::LessEqual:
                return "<=";
            case ir::IrArithmeticOperator::Greater:
                return ">";
            case ir::IrArithmeticOperator::GreaterEqual:
                return ">=";
            case ir::IrArithmeticOperator::LogicalAnd:
                return "&&";
            case ir::IrArithmeticOperator::LogicalOr:
                return "||";
            case ir::IrArithmeticOperator::Negate:
            case ir::IrArithmeticOperator::Not:
                failInvariant("Unary operation reached binary expression.");
        }

        failInvariant("Unknown binary operation.");
    }

    // Raises a deterministic unsupported-backend diagnostic for one IR feature.
    [[noreturn]] void KotlinExpressionEmitter::failUnsupported(
        const char* feature
    ) {
        throw runtime_error(
            string("Kotlin generation does not support ") + feature + "."
        );
    }

    // Raises a deterministic internal invariant diagnostic for malformed IR.
    [[noreturn]] void KotlinExpressionEmitter::failInvariant(
        const char* message
    ) {
        throw runtime_error(
            string("Kotlin generation invariant failed: ") + message
        );
    }

}
