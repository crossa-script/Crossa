#include "crossa/compiler/generators/native/NativeProgramGenerator.h"

#include <sstream>
#include <stdexcept>

using namespace std;

namespace crossa::compiler::generators::native {

    class NativeProgramEmitter final {
    public:
        static string quote(const string& value) {
            string result = "\"";
            for (const char character : value) {
                if (character == '\\' || character == '\"') result += '\\';
                if (character == '\n') result += "\\n";
                else result += character;
            }
            return result + "\"";
        }

        static string type(const types::SemanticType& value) {
            switch (value.getKind()) {
                case types::SemanticTypeKind::Unit: return "SemanticType::createUnit()";
                case types::SemanticTypeKind::Int: return "SemanticType::createInt()";
                case types::SemanticTypeKind::Long: return "SemanticType::createLong()";
                case types::SemanticTypeKind::Double: return "SemanticType::createDouble()";
                case types::SemanticTypeKind::String: return "SemanticType::createString()";
                case types::SemanticTypeKind::Bool: return "SemanticType::createBool()";
                case types::SemanticTypeKind::Json: return "SemanticType::createJson()";
                case types::SemanticTypeKind::Model: return "SemanticType::createModel(" + quote(value.getModelName()) + ")";
                case types::SemanticTypeKind::List: return "SemanticType::createList(" + type(*value.getElementType()) + ")";
            }
            throw runtime_error("Unsupported generated semantic type.");
        }

        static string expression(const ir::IrExpression& value) {
            const string location = "SourceLocation(1, 1)";
            switch (value.getKind()) {
                case ir::IrExpressionKind::ReadSymbol: {
                    const auto& node = static_cast<const ir::IrReadSymbolExpression&>(value);
                    return "make_unique<IrReadSymbolExpression>(" + quote(node.getName()) + ", IrSymbolKind::" + symbol(node.getSymbolKind()) + ", " + type(node.getType()) + ", " + location + ")";
                }
                case ir::IrExpressionKind::IntegerConstant: {
                    const auto& node = static_cast<const ir::IrIntegerConstantExpression&>(value);
                    return "make_unique<IrIntegerConstantExpression>(" + quote(node.getValue()) + ", " + type(node.getType()) + ", " + location + ")";
                }
                case ir::IrExpressionKind::DoubleConstant: {
                    const auto& node = static_cast<const ir::IrDoubleConstantExpression&>(value);
                    return "make_unique<IrDoubleConstantExpression>(" + quote(node.getValue()) + ", " + location + ")";
                }
                case ir::IrExpressionKind::BooleanConstant: {
                    const auto& node = static_cast<const ir::IrBooleanConstantExpression&>(value);
                    return "make_unique<IrBooleanConstantExpression>(" + string(node.getValue() ? "true" : "false") + ", " + location + ")";
                }
                case ir::IrExpressionKind::Unary: {
                    const auto& node = static_cast<const ir::IrUnaryExpression&>(value);
                    return "make_unique<IrUnaryExpression>(IrArithmeticOperator::" + arithmetic(node.getOperator()) + ", " + expression(node.getOperand()) + ", " + type(node.getType()) + ", " + location + ")";
                }
                case ir::IrExpressionKind::Binary: {
                    const auto& node = static_cast<const ir::IrBinaryExpression&>(value);
                    return "make_unique<IrBinaryExpression>(" + expression(node.getLeft()) + ", IrArithmeticOperator::" + arithmetic(node.getOperator()) + ", " + expression(node.getRight()) + ", " + type(node.getType()) + ", " + location + ")";
                }
                default: throw runtime_error("Android native program generation does not support this IR expression.");
            }
        }

        static string statement(const ir::IrStatement& value) {
            const string location = "SourceLocation(1, 1)";
            if (value.getKind() == ir::IrStatementKind::Return) {
                const auto& node = static_cast<const ir::IrReturnStatement&>(value);
                return "make_unique<IrReturnStatement>(" + expression(node.getExpression()) + ", " + location + ")";
            }
            if (value.getKind() == ir::IrStatementKind::Local) {
                const auto& node = static_cast<const ir::IrLocalStatement&>(value);
                return "make_unique<IrLocalStatement>(" + quote(node.getName()) + ", " + type(node.getType()) + ", " + expression(node.getInitializer()) + ", " + location + ")";
            }
            throw runtime_error("Android native program generation does not support this IR statement.");
        }

    private:
        static string arithmetic(ir::IrArithmeticOperator value) { return value == ir::IrArithmeticOperator::Add ? "Add" : value == ir::IrArithmeticOperator::Subtract ? "Subtract" : value == ir::IrArithmeticOperator::Multiply ? "Multiply" : value == ir::IrArithmeticOperator::Divide ? "Divide" : value == ir::IrArithmeticOperator::Negate ? "Negate" : value == ir::IrArithmeticOperator::Equal ? "Equal" : value == ir::IrArithmeticOperator::NotEqual ? "NotEqual" : value == ir::IrArithmeticOperator::Less ? "Less" : value == ir::IrArithmeticOperator::LessEqual ? "LessEqual" : value == ir::IrArithmeticOperator::Greater ? "Greater" : value == ir::IrArithmeticOperator::GreaterEqual ? "GreaterEqual" : value == ir::IrArithmeticOperator::LogicalAnd ? "LogicalAnd" : value == ir::IrArithmeticOperator::LogicalOr ? "LogicalOr" : "Not"; }
        static string symbol(ir::IrSymbolKind value) { return value == ir::IrSymbolKind::Parameter ? "Parameter" : value == ir::IrSymbolKind::LocalVariable ? "LocalVariable" : "SourceVariable"; }
    };

    // Generates the immutable operation identifier declarations for one program.
    string NativeProgramGenerator::generateOperationHeader(
        const ir::Program& program
    ) const {
        ostringstream output;
        output << "#pragma once\n\n#include <cstdint>\n\n";
        output << "namespace crossa::generated {\n\n";
        output << "class GeneratedOperations final {\npublic:\n";
        for (const unique_ptr<ir::IrDeclaration>& declaration :
             program.getDeclarations()) {
            if (declaration->getKind() != ir::IrDeclarationKind::Function) {
                continue;
            }
            const auto& function = static_cast<const ir::IrFunctionDeclaration&>(
                *declaration
            );
            output << "    static constexpr std::uint64_t "
                   << function.getName() << " = "
                   << operationId(program.getIdentity(), function)
                   << "ULL;\n";
        }
        output << "};\n\n}\n";
        return output.str();
    }

    // Generates the native declaration for the embedded executable program.
    string NativeProgramGenerator::generateProgramHeader() const {
        return "#pragma once\n\n#include <crossa/compiler/ir/Program.h>\n\nnamespace crossa::generated {\n\nclass CrossaGeneratedProgram final {\npublic:\n    static compiler::ir::Program create();\n};\n\n}\n";
    }

    // Generates the executable Program reconstruction source for one IR program.
    string NativeProgramGenerator::generateProgramSource(const ir::Program& program) const {
        ostringstream output;
        output << "#include \"CrossaGeneratedProgram.h\"\n#include <memory>\n#include <vector>\n#include <crossa/compiler/ir/IrDeclaration.h>\n#include <crossa/compiler/source/SourceLocation.h>\n\nusing namespace std;\nnamespace crossa::generated {\ncompiler::ir::Program CrossaGeneratedProgram::create() {\nusing namespace compiler::ir; using namespace compiler::types; using compiler::source::SourceLocation;\nvector<unique_ptr<IrDeclaration>> declarations;\n";
        for (const unique_ptr<ir::IrDeclaration>& declaration : program.getDeclarations()) {
            if (declaration->getKind() != ir::IrDeclarationKind::Function) continue;
            const auto& function = static_cast<const ir::IrFunctionDeclaration&>(*declaration);
            if (function.getExecutionPolicy() == ir::IrExecutionPolicy::Sync) continue;
            output << "{ vector<IrParameter> parameters;\n";
            for (const ir::IrParameter& parameter : function.getParameters()) output << "parameters.emplace_back(" << NativeProgramEmitter::quote(parameter.getName()) << ", " << NativeProgramEmitter::type(parameter.getType()) << ", SourceLocation(1, 1));\n";
            output << "vector<unique_ptr<IrStatement>> statements;\n";
            for (const unique_ptr<ir::IrStatement>& statement : function.getStatements()) output << "statements.push_back(" << NativeProgramEmitter::statement(*statement) << ");\n";
            output << "declarations.push_back(make_unique<IrFunctionDeclaration>(" << NativeProgramEmitter::quote(function.getName()) << ", IrExecutionPolicy::" << (function.getExecutionPolicy() == ir::IrExecutionPolicy::AsyncAfter ? "AsyncAfter" : function.getExecutionPolicy() == ir::IrExecutionPolicy::Async ? "Async" : "Sync") << ", move(parameters), " << NativeProgramEmitter::type(function.getReturnType()) << ", move(statements), SourceLocation(1, 1))); }\n";
        }
        output << "return compiler::ir::Program({}, " << NativeProgramEmitter::quote(program.getIdentity()) << ", move(declarations)); }\n}\n";
        return output.str();
    }

    // Computes the canonical stable operation identifier used by native bindings.
    uint64_t NativeProgramGenerator::operationId(
        const string& sourceIdentity,
        const ir::IrFunctionDeclaration& function
    ) noexcept {
        uint64_t value = 1469598103934665603ULL;
        const string signature = sourceIdentity + ":" + function.getName() +
            ":" + function.getReturnType().format();
        for (const char character : signature) {
            value ^= static_cast<unsigned char>(character);
            value *= 1099511628211ULL;
        }
        return value;
    }

}
