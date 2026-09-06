#include "crossa/compiler/generators/swift/SwiftGenerator.h"

#include <cstdint>
#include <filesystem>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string_view>

#include "crossa/compiler/ir/IrJsonExpression.h"

using namespace std;

namespace crossa::compiler::generators::swift {

    // Provides deterministic planning and emission helpers for Swift project outputs.
    // generate() coordinates indexed declarations while emitExpression() preserves IR semantics.
    class SwiftGenerationSupport final {
    public:
        // Generates every model and source-owned API file from linked IR programs.
        [[nodiscard]] static vector<SwiftGeneratedSource> generate(
            const vector<const ir::Program*>& programs
        ) {
            if (programs.empty()) {
                throw runtime_error("Swift project generation requires linked IR.");
            }
            map<string, const ir::IrModelDeclaration*> models;
            map<string, vector<const ir::IrFunctionDeclaration*>> functions;
            map<string, string> modelKeys;
            set<string> functionKeys;
            set<string> functionSymbols;
            for (const ir::Program* program : programs) {
                if (program == nullptr) {
                    throw runtime_error("Swift project generation received null IR.");
                }
                indexProgram(
                    *program,
                    models,
                    functions,
                    modelKeys,
                    functionKeys,
                    functionSymbols
                );
            }

            vector<SwiftGeneratedSource> output;
            output.reserve(models.size() + functions.size() + 1);
            output.emplace_back("api/CrossaFunctions.swift", generatedHeader() +
                "import Foundation\n\npublic enum CrossaFunctions {}\n");
            for (const auto& entry : models) {
                output.emplace_back(
                    "model/" + escapePath(entry.first) + ".swift",
                    emitModel(*entry.second)
                );
            }
            set<string> fileNames;
            for (const auto& entry : functions) {
                const string sourceName = pascalCase(
                    filesystem::path(entry.first).stem().string()
                );
                const string path = "api/" + sourceName + ".swift";
                if (!fileNames.insert(path).second) {
                    throw runtime_error(
                        "Swift generation found conflicting source output '" +
                        path + "'."
                    );
                }
                output.emplace_back(path, emitFunctions(entry.second, models));
            }
            return output;
        }

    private:
        // Indexes one linked program while retaining only canonical declarations.
        static void indexProgram(
            const ir::Program& program,
            map<string, const ir::IrModelDeclaration*>& models,
            map<string, vector<const ir::IrFunctionDeclaration*>>& functions,
            map<string, string>& modelKeys,
            set<string>& functionKeys,
            set<string>& functionSymbols
        ) {
            for (const unique_ptr<ir::IrDeclaration>& declaration :
                 program.getDeclarations()) {
                const string sourceUnit = sourceKey(*declaration, program);
                const string key = declarationKey(*declaration, program);
                if (declaration->getKind() == ir::IrDeclarationKind::Model) {
                    const auto& model = static_cast<const ir::IrModelDeclaration&>(
                        *declaration
                    );
                    const auto existing = modelKeys.find(model.getName());
                    if (existing != modelKeys.end() && existing->second != key) {
                        throw runtime_error(
                            "Swift generation found duplicate model symbol '" +
                            model.getName() + "'."
                        );
                    }
                    modelKeys.emplace(model.getName(), key);
                    models.emplace(model.getName(), &model);
                    continue;
                }
                if (declaration->getKind() != ir::IrDeclarationKind::Function ||
                    !functionKeys.insert(key).second) {
                    continue;
                }
                const auto& function = static_cast<const ir::IrFunctionDeclaration&>(
                    *declaration
                );
                if (!functionSymbols.insert(function.getName()).second) {
                    throw runtime_error(
                        "Swift generation found duplicate function symbol '" +
                        function.getName() + "'."
                    );
                }
                functions[sourceUnit].push_back(&function);
            }
        }

        // Returns one declaration identity that is stable across linked imports.
        [[nodiscard]] static string declarationKey(
            const ir::IrDeclaration& declaration,
            const ir::Program& program
        ) {
            const source::SourceLocation& location = declaration.getLocation();
            const string sourcePath = location.getSourcePath().empty()
                ? program.getIdentity()
                : string(location.getSourcePath());
            return sourcePath + ":" + to_string(location.getLine()) + ":" +
                to_string(location.getColumn()) + ":" +
                to_string(static_cast<int>(declaration.getKind()));
        }

        // Returns the source path used to group one function into a separate API file.
        [[nodiscard]] static string sourceKey(
            const ir::IrDeclaration& declaration,
            const ir::Program& program
        ) {
            return declaration.getLocation().getSourcePath().empty()
                ? program.getIdentity()
                : string(declaration.getLocation().getSourcePath());
        }

        // Returns the standard deterministic preamble for every generated Swift file.
        [[nodiscard]] static string generatedHeader() {
            return "// Generated by Crossa. Do not edit.\n\n";
        }

        // Converts a source identity into a stable PascalCase Swift file stem.
        [[nodiscard]] static string pascalCase(string_view value) {
            string result;
            bool capitalize = true;
            for (const char character : value) {
                const bool letter = (character >= 'a' && character <= 'z') ||
                    (character >= 'A' && character <= 'Z');
                const bool digit = character >= '0' && character <= '9';
                if (!letter && !digit) {
                    capitalize = true;
                    continue;
                }
                if (result.empty() && digit) result = "Crossa";
                result += capitalize && character >= 'a' && character <= 'z'
                    ? static_cast<char>(character - ('a' - 'A'))
                    : character;
                capitalize = false;
            }
            if (result.empty()) {
                throw runtime_error("Swift generation cannot derive a source name.");
            }
            return result;
        }

        // Returns a path-safe generated file component for a validated model name.
        [[nodiscard]] static string escapePath(const string& value) {
            if (value.empty() || value.find('/') != string::npos ||
                value.find('\\') != string::npos) {
                throw runtime_error("Swift generation encountered an invalid model name.");
            }
            return value;
        }

        // Emits one native-backed Swift model with schema-order field accessors.
        [[nodiscard]] static string emitModel(const ir::IrModelDeclaration& model) {
            ostringstream output;
            output << generatedHeader() << "import Foundation\n\n";
            output << "public struct " << identifier(model.getName())
                   << ": @unchecked Sendable {\n";
            output << "    internal let nativeValue: CrossaNativeValue\n\n";
            const vector<ir::IrModelField>& fields = model.getFields();
            for (size_t index = 0; index < fields.size(); ++index) {
                const ir::IrModelField& field = fields[index];
                output << "    public var " << identifier(field.getName()) << ": "
                       << type(field.getType()) << " {\n";
                output << "        " << modelFieldValue(
                    field.getType(), to_string(index)
                ) << "\n";
                output << "    }\n";
                if (index + 1 < fields.size()) output << "\n";
            }
            output << "}\n";
            return output.str();
        }

        // Emits all functions whose original source unit shares one output file.
        [[nodiscard]] static string emitFunctions(
            const vector<const ir::IrFunctionDeclaration*>& functions,
            const map<string, const ir::IrModelDeclaration*>& models
        ) {
            ostringstream output;
            output << generatedHeader() << "import Foundation\n\n";
            output << "public extension CrossaFunctions {\n";
            for (size_t index = 0; index < functions.size(); ++index) {
                emitFunction(*functions[index], models, output);
                if (index + 1 < functions.size()) output << "\n";
            }
            output << "}\n";
            return output.str();
        }

        // Emits one pure Swift function or a thin runtime-backed operation wrapper.
        static void emitFunction(
            const ir::IrFunctionDeclaration& function,
            const map<string, const ir::IrModelDeclaration*>& models,
            ostringstream& output
        ) {
            (void)models;
            if (function.getExecutionPolicy() != ir::IrExecutionPolicy::Sync) {
                emitNativeFunction(function, output);
                return;
            }
            validatePureStatements(function.getStatements());
            output << "    static func " << identifier(function.getName())
                   << "(";
            emitParameters(function.getParameters(), output, false);
            output << ")";
            if (function.getReturnType().getKind() != types::SemanticTypeKind::Unit) {
                output << " -> " << type(function.getReturnType());
            }
            output << " {\n";
            emitStatements(function.getStatements(), output, 2);
            output << "    }\n";
        }

        // Emits one async wrapper that delegates scheduling and execution to native code.
        static void emitNativeFunction(
            const ir::IrFunctionDeclaration& function,
            ostringstream& output
        ) {
            output << "    @discardableResult\n";
            output << "    static func " << identifier(function.getName())
                   << "(runtime: CrossaRuntime";
            if (!function.getParameters().empty()) output << ", ";
            emitParameters(function.getParameters(), output, false);
            if (function.getExecutionPolicy() == ir::IrExecutionPolicy::AsyncAfter) {
                output << ", ";
                output << "onState: @escaping (CrossaState<" << type(
                    function.getReturnType()
                ) << ">) -> Void";
            }
            output << ") -> CrossaOperation {\n";
            output << "        let arguments: [CrossaArgument] = [";
            const vector<ir::IrParameter>& parameters = function.getParameters();
            for (size_t index = 0; index < parameters.size(); ++index) {
                if (index > 0) output << ", ";
                output << "CrossaArgument(" << identifier(parameters[index].getName())
                       << ")";
            }
            output << "]\n";
            const string operation = to_string(operationId(function));
            if (function.getExecutionPolicy() == ir::IrExecutionPolicy::Async) {
                output << "        return runtime.invokeAsync(operation: " << operation
                       << ", arguments: arguments)\n";
            } else {
                output << "        return runtime.invokeAsyncAfter(operation: " << operation
                       << ", arguments: arguments, map: { result in\n";
                output << "            " << resultMapper(function.getReturnType()) << "\n";
                output << "        }, onState: onState)\n";
            }
            output << "    }\n";
            if (function.getExecutionPolicy() == ir::IrExecutionPolicy::AsyncAfter) {
                output << "\n    static func " << identifier(function.getName())
                       << "(runtime: CrossaRuntime";
                if (!function.getParameters().empty()) output << ", ";
                emitParameters(function.getParameters(), output, false);
                output << ") async throws -> " << type(function.getReturnType()) << " {\n";
                output << "        let arguments: [CrossaArgument] = [";
                for (size_t index = 0; index < parameters.size(); ++index) {
                    if (index > 0) output << ", ";
                    output << "CrossaArgument(" << identifier(parameters[index].getName())
                           << ")";
                }
                output << "]\n";
                output << "        return try await runtime.invokeAsyncAfterAwait(operation: "
                       << operation << ", arguments: arguments, map: { result in\n";
                output << "            " << resultMapper(function.getReturnType()) << "\n";
                output << "        })\n";
                output << "    }\n";
            }
        }

        // Emits a deterministic function parameter list.
        static void emitParameters(
            const vector<ir::IrParameter>& parameters,
            ostringstream& output,
            bool includeLabels
        ) {
            for (size_t index = 0; index < parameters.size(); ++index) {
                if (index > 0) output << ", ";
                const string name = identifier(parameters[index].getName());
                if (includeLabels) output << name << " ";
                output << name << ": " << type(parameters[index].getType());
            }
        }

        // Emits every statement from a pure Crossa function with Swift indentation.
        static void emitStatements(
            const vector<unique_ptr<ir::IrStatement>>& statements,
            ostringstream& output,
            size_t indentation
        ) {
            for (const unique_ptr<ir::IrStatement>& statement : statements) {
                emitStatement(*statement, output, indentation);
            }
        }

        // Emits one pure Crossa statement while retaining branch structure.
        static void emitStatement(
            const ir::IrStatement& statement,
            ostringstream& output,
            size_t indentation
        ) {
            const string spaces(indentation * 4, ' ');
            switch (statement.getKind()) {
                case ir::IrStatementKind::Return:
                    output << spaces << "return " << expression(
                        static_cast<const ir::IrReturnStatement&>(statement)
                            .getExpression()
                    ) << "\n";
                    return;
                case ir::IrStatementKind::Evaluate:
                    output << spaces << expression(
                        static_cast<const ir::IrEvaluateStatement&>(statement)
                            .getExpression()
                    ) << "\n";
                    return;
                case ir::IrStatementKind::Local: {
                    const auto& local = static_cast<const ir::IrLocalStatement&>(
                        statement
                    );
                    output << spaces << "let " << identifier(local.getName())
                           << ": " << type(local.getType()) << " = "
                           << expression(local.getInitializer()) << "\n";
                    return;
                }
                case ir::IrStatementKind::If:
                    emitConditional(
                        static_cast<const ir::IrIfStatement&>(statement),
                        output,
                        indentation,
                        false
                    );
                    return;
            }
            throw runtime_error("Swift generation encountered an unknown statement.");
        }

        // Emits one if, else-if, or else branch without changing Crossa evaluation order.
        static void emitConditional(
            const ir::IrIfStatement& statement,
            ostringstream& output,
            size_t indentation,
            bool elseIf
        ) {
            const string spaces(indentation * 4, ' ');
            output << spaces << (elseIf ? "else if " : "if ")
                   << expression(statement.getCondition()) << " {\n";
            emitStatements(statement.getThenStatements(), output, indentation + 1);
            output << spaces << "}";
            const vector<unique_ptr<ir::IrStatement>>* alternative =
                statement.getElseStatements();
            if (alternative == nullptr) {
                output << "\n";
                return;
            }
            if (alternative->size() == 1 &&
                alternative->front()->getKind() == ir::IrStatementKind::If) {
                output << " ";
                emitConditional(
                    static_cast<const ir::IrIfStatement&>(*alternative->front()),
                    output,
                    indentation,
                    true
                );
                return;
            }
            output << " else {\n";
            emitStatements(*alternative, output, indentation + 1);
            output << spaces << "}\n";
        }

        // Emits one Swift expression from fully typed Crossa IR.
        [[nodiscard]] static string expression(const ir::IrExpression& value) {
            switch (value.getKind()) {
                case ir::IrExpressionKind::ReadSymbol:
                    return identifier(static_cast<const ir::IrReadSymbolExpression&>(
                        value
                    ).getName());
                case ir::IrExpressionKind::IntegerConstant:
                    return static_cast<const ir::IrIntegerConstantExpression&>(
                        value
                    ).getValue();
                case ir::IrExpressionKind::DoubleConstant:
                    return static_cast<const ir::IrDoubleConstantExpression&>(
                        value
                    ).getValue();
                case ir::IrExpressionKind::BooleanConstant:
                    return static_cast<const ir::IrBooleanConstantExpression&>(
                        value
                    ).getValue() ? "true" : "false";
                case ir::IrExpressionKind::StringBuild:
                    return stringBuild(static_cast<const ir::IrStringBuildExpression&>(
                        value
                    ));
                case ir::IrExpressionKind::Call:
                    return call(static_cast<const ir::IrCallExpression&>(value));
                case ir::IrExpressionKind::Unary: {
                    const auto& unary = static_cast<const ir::IrUnaryExpression&>(value);
                    return unaryOperator(unary.getOperator()) + "(" +
                        expression(unary.getOperand()) + ")";
                }
                case ir::IrExpressionKind::Binary: {
                    const auto& binary = static_cast<const ir::IrBinaryExpression&>(value);
                    return "(" + expression(binary.getLeft()) + " " +
                        binaryOperator(binary.getOperator()) + " " +
                        expression(binary.getRight()) + ")";
                }
                case ir::IrExpressionKind::CrossaRequest:
                    throw runtime_error(
                        "Swift pure generation does not support CrossaRequest."
                    );
                case ir::IrExpressionKind::JsonNumber:
                case ir::IrExpressionKind::JsonNull:
                case ir::IrExpressionKind::JsonObject:
                case ir::IrExpressionKind::JsonArray:
                    throw runtime_error("Swift pure generation does not support JSON expressions.");
            }
            throw runtime_error("Swift generation encountered an unknown expression.");
        }

        // Emits one Swift call while keeping Crossa functions in their common namespace.
        [[nodiscard]] static string call(const ir::IrCallExpression& call) {
            ostringstream output;
            output << (call.isBuiltin() ? "print" : "CrossaFunctions." +
                identifier(call.getCallee())) << "(";
            for (size_t index = 0; index < call.getArguments().size(); ++index) {
                if (index > 0) output << ", ";
                output << expression(*call.getArguments()[index]);
            }
            output << ")";
            return output.str();
        }

        // Emits a Swift string literal or interpolation expression from resolved segments.
        [[nodiscard]] static string stringBuild(const ir::IrStringBuildExpression& value) {
            ostringstream output;
            output << '"';
            for (const ir::IrStringSegment& segment : value.getSegments()) {
                if (segment.getKind() == ir::IrStringSegmentKind::Symbol) {
                    output << "\\(" << identifier(segment.getValue()) << ")";
                } else {
                    output << escapeString(segment.getValue());
                }
            }
            output << '"';
            return output.str();
        }

        // Escapes one literal source segment for a Swift string literal.
        [[nodiscard]] static string escapeString(const string& value) {
            string output;
            for (const char character : value) {
                switch (character) {
                    case '\\': output += "\\\\"; break;
                    case '"': output += "\\\""; break;
                    case '\n': output += "\\n"; break;
                    case '\r': output += "\\r"; break;
                    case '\t': output += "\\t"; break;
                    default: output += character; break;
                }
            }
            return output;
        }

        // Maps a typed Crossa value type into its public Swift representation.
        [[nodiscard]] static string type(const types::SemanticType& value) {
            switch (value.getKind()) {
                case types::SemanticTypeKind::Int: return "Int";
                case types::SemanticTypeKind::Long: return "Int64";
                case types::SemanticTypeKind::Double: return "Double";
                case types::SemanticTypeKind::String: return "String";
                case types::SemanticTypeKind::Bool: return "Bool";
                case types::SemanticTypeKind::Unit: return "Void";
                case types::SemanticTypeKind::Model: return identifier(value.getModelName());
                case types::SemanticTypeKind::List:
                    if (value.getElementType() == nullptr) break;
                    return "CrossaList<" + type(*value.getElementType()) + ">";
                case types::SemanticTypeKind::Json:
                    break;
            }
            throw runtime_error("Swift generation does not support type '" +
                value.format() + "'.");
        }

        // Returns native-backed property code for one model field index.
        [[nodiscard]] static string modelFieldValue(
            const types::SemanticType& fieldType,
            const string& fieldIndex
        ) {
            const string child = "nativeValue.child(field: " + fieldIndex + ")";
            switch (fieldType.getKind()) {
                case types::SemanticTypeKind::Int: return child + ".int()";
                case types::SemanticTypeKind::Long: return child + ".long()";
                case types::SemanticTypeKind::Double: return child + ".double()";
                case types::SemanticTypeKind::String: return child + ".string()";
                case types::SemanticTypeKind::Bool: return child + ".bool()";
                case types::SemanticTypeKind::Model:
                    return identifier(fieldType.getModelName()) +
                        "(nativeValue: " + child + ")";
                case types::SemanticTypeKind::List:
                    if (fieldType.getElementType() == nullptr) break;
                    return "CrossaList(nativeValue: " + child + ", map: { value in " +
                        valueMapper(*fieldType.getElementType(), "value") + " })";
                case types::SemanticTypeKind::Unit:
                case types::SemanticTypeKind::Json:
                    break;
            }
            throw runtime_error("Swift generation does not support a native model field type.");
        }

        // Returns the closure expression that maps a native value into one Swift type.
        [[nodiscard]] static string valueMapper(
            const types::SemanticType& valueType,
            const string& valueName
        ) {
            switch (valueType.getKind()) {
                case types::SemanticTypeKind::Int: return valueName + ".int()";
                case types::SemanticTypeKind::Long: return valueName + ".long()";
                case types::SemanticTypeKind::Double: return valueName + ".double()";
                case types::SemanticTypeKind::String: return valueName + ".string()";
                case types::SemanticTypeKind::Bool: return valueName + ".bool()";
                case types::SemanticTypeKind::Model:
                    return identifier(valueType.getModelName()) +
                        "(nativeValue: " + valueName + ")";
                case types::SemanticTypeKind::List:
                    if (valueType.getElementType() == nullptr) break;
                    return "CrossaList(nativeValue: " + valueName + ", map: { item in " +
                        valueMapper(*valueType.getElementType(), "item") + " })";
                case types::SemanticTypeKind::Unit:
                case types::SemanticTypeKind::Json:
                    break;
            }
            throw runtime_error("Swift generation does not support a native result type.");
        }

        // Returns the AsyncAfter closure body that owns scalar and native result lifetime.
        [[nodiscard]] static string resultMapper(const types::SemanticType& valueType) {
            switch (valueType.getKind()) {
                case types::SemanticTypeKind::Int:
                    return "result.takeInt()";
                case types::SemanticTypeKind::Long:
                    return "result.takeLong()";
                case types::SemanticTypeKind::Double:
                    return "result.takeDouble()";
                case types::SemanticTypeKind::String:
                    return "result.takeString()";
                case types::SemanticTypeKind::Bool:
                    return "result.takeBool()";
                case types::SemanticTypeKind::Model:
                    return identifier(valueType.getModelName()) +
                        "(nativeValue: result.rootValue())";
                case types::SemanticTypeKind::List:
                    if (valueType.getElementType() == nullptr) break;
                    return "CrossaList(nativeValue: result.rootValue(), map: { value in " +
                        valueMapper(*valueType.getElementType(), "value") + " })";
                case types::SemanticTypeKind::Unit:
                    return "result.release(); return ()";
                case types::SemanticTypeKind::Json:
                    break;
            }
            throw runtime_error("Swift generation does not support an AsyncAfter result type.");
        }

        // Derives the canonical FNV-1a operation identifier used by generated native code.
        [[nodiscard]] static uint64_t operationId(
            const ir::IrFunctionDeclaration& function
        ) noexcept {
            const string_view sourcePath = function.getLocation().getSourcePath();
            const string sourceIdentity = sourcePath.empty() ? "CrossaGenerated" :
                filesystem::path(string(sourcePath)).stem().string();
            uint64_t value = 1469598103934665603ULL;
            const string signature = sourceIdentity + ":" + function.getName() +
                ":" + function.getReturnType().format();
            for (const char character : signature) {
                value ^= static_cast<unsigned char>(character);
                value *= 1099511628211ULL;
            }
            return value;
        }

        // Escapes one Crossa identifier for Swift source without relying on reflection.
        [[nodiscard]] static string identifier(const string& value) {
            static const set<string> keywords = {
                "as", "associatedtype", "break", "case", "catch", "class", "continue",
                "default", "defer", "deinit", "do", "else", "enum", "extension", "fallthrough",
                "false", "fileprivate", "for", "func", "guard", "if", "import", "in", "init",
                "inout", "internal", "is", "let", "nil", "open", "operator", "private", "protocol",
                "public", "repeat", "rethrows", "return", "self", "static", "struct", "subscript",
                "super", "switch", "throw", "throws", "true", "try", "typealias", "var", "where",
                "while"
            };
            if (value.empty()) throw runtime_error("Swift generation encountered an empty identifier.");
            const bool firstValid = (value.front() >= 'a' && value.front() <= 'z') ||
                (value.front() >= 'A' && value.front() <= 'Z') || value.front() == '_';
            bool valid = firstValid;
            for (const char character : value) {
                valid = valid && ((character >= 'a' && character <= 'z') ||
                    (character >= 'A' && character <= 'Z') ||
                    (character >= '0' && character <= '9') || character == '_');
            }
            return valid && keywords.find(value) == keywords.end()
                ? value
                : "`" + value + "`";
        }

        // Maps a Crossa unary operator into its Swift equivalent.
        [[nodiscard]] static string unaryOperator(ir::IrArithmeticOperator value) {
            return value == ir::IrArithmeticOperator::Negate ? "-" :
                value == ir::IrArithmeticOperator::Not ? "!" :
                throw runtime_error("Swift generation received a binary unary operator.");
        }

        // Maps a Crossa binary operator into its Swift equivalent.
        [[nodiscard]] static string binaryOperator(ir::IrArithmeticOperator value) {
            switch (value) {
                case ir::IrArithmeticOperator::Add: return "+";
                case ir::IrArithmeticOperator::Subtract: return "-";
                case ir::IrArithmeticOperator::Multiply: return "*";
                case ir::IrArithmeticOperator::Divide: return "/";
                case ir::IrArithmeticOperator::Equal: return "==";
                case ir::IrArithmeticOperator::NotEqual: return "!=";
                case ir::IrArithmeticOperator::Less: return "<";
                case ir::IrArithmeticOperator::LessEqual: return "<=";
                case ir::IrArithmeticOperator::Greater: return ">";
                case ir::IrArithmeticOperator::GreaterEqual: return ">=";
                case ir::IrArithmeticOperator::LogicalAnd: return "&&";
                case ir::IrArithmeticOperator::LogicalOr: return "||";
                case ir::IrArithmeticOperator::Negate:
                case ir::IrArithmeticOperator::Not:
                    break;
            }
            throw runtime_error("Swift generation received a unary binary operator.");
        }

        // Rejects native work in a pure translated synchronous function.
        static void validatePureStatements(
            const vector<unique_ptr<ir::IrStatement>>& statements
        ) {
            for (const unique_ptr<ir::IrStatement>& statement : statements) {
                validatePureStatement(*statement);
            }
        }

        // Rejects one runtime-backed statement from pure Swift translation.
        static void validatePureStatement(const ir::IrStatement& statement) {
            switch (statement.getKind()) {
                case ir::IrStatementKind::Return:
                    validatePureExpression(static_cast<const ir::IrReturnStatement&>(
                        statement
                    ).getExpression());
                    return;
                case ir::IrStatementKind::Evaluate:
                    validatePureExpression(static_cast<const ir::IrEvaluateStatement&>(
                        statement
                    ).getExpression());
                    return;
                case ir::IrStatementKind::Local:
                    validatePureExpression(static_cast<const ir::IrLocalStatement&>(
                        statement
                    ).getInitializer());
                    return;
                case ir::IrStatementKind::If: {
                    const auto& conditional = static_cast<const ir::IrIfStatement&>(
                        statement
                    );
                    validatePureExpression(conditional.getCondition());
                    validatePureStatements(conditional.getThenStatements());
                    if (conditional.getElseStatements() != nullptr) {
                        validatePureStatements(*conditional.getElseStatements());
                    }
                    return;
                }
            }
            throw runtime_error("Swift generation encountered an unknown statement.");
        }

        // Rejects runtime-backed expressions from pure Swift translation.
        static void validatePureExpression(const ir::IrExpression& expression) {
            switch (expression.getKind()) {
                case ir::IrExpressionKind::CrossaRequest:
                    throw runtime_error("Swift pure generation does not support CrossaRequest.");
                case ir::IrExpressionKind::Call: {
                    const auto& call = static_cast<const ir::IrCallExpression&>(expression);
                    for (const unique_ptr<ir::IrExpression>& argument : call.getArguments()) {
                        validatePureExpression(*argument);
                    }
                    return;
                }
                case ir::IrExpressionKind::Unary:
                    validatePureExpression(static_cast<const ir::IrUnaryExpression&>(
                        expression
                    ).getOperand());
                    return;
                case ir::IrExpressionKind::Binary: {
                    const auto& binary = static_cast<const ir::IrBinaryExpression&>(
                        expression
                    );
                    validatePureExpression(binary.getLeft());
                    validatePureExpression(binary.getRight());
                    return;
                }
                case ir::IrExpressionKind::JsonObject:
                case ir::IrExpressionKind::JsonArray:
                case ir::IrExpressionKind::JsonNumber:
                case ir::IrExpressionKind::JsonNull:
                    throw runtime_error("Swift pure generation does not support JSON expressions.");
                case ir::IrExpressionKind::ReadSymbol:
                case ir::IrExpressionKind::IntegerConstant:
                case ir::IrExpressionKind::DoubleConstant:
                case ir::IrExpressionKind::StringBuild:
                case ir::IrExpressionKind::BooleanConstant:
                    return;
            }
            throw runtime_error("Swift generation encountered an unknown expression.");
        }
    };

    // Generates separated Swift model and API files for one linked Crossa project.
    vector<SwiftGeneratedSource> SwiftGenerator::generateProject(
        const vector<const ir::Program*>& programs
    ) const {
        return SwiftGenerationSupport::generate(programs);
    }

}
