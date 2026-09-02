#include "crossa/compiler/generators/native/NativeProgramGenerator.h"

#include <filesystem>
#include <map>
#include <optional>
#include <set>
#include <sstream>
#include <stdexcept>

#include "crossa/compiler/ir/IrCrossaRequestExpression.h"
#include "crossa/compiler/ir/IrJsonExpression.h"

using namespace std;

namespace crossa::compiler::generators::native {

    // Reconstructs every supported Crossa IR node as deterministic native C++ source.
    class NativeProgramEmitter final {
    public:
        // Quotes source text for one generated C++ string literal.
        [[nodiscard]] static string quote(const string& value) {
            string result = "\"";
            for (const char character : value) {
                switch (character) {
                    case '\\': result += "\\\\"; break;
                    case '\"': result += "\\\""; break;
                    case '\n': result += "\\n"; break;
                    case '\r': result += "\\r"; break;
                    case '\t': result += "\\t"; break;
                    default: result += character; break;
                }
            }
            return result + "\"";
        }

        // Serializes one resolved semantic type.
        [[nodiscard]] static string type(const types::SemanticType& value) {
            switch (value.getKind()) {
                case types::SemanticTypeKind::Unit: return "SemanticType::createUnit()";
                case types::SemanticTypeKind::Int: return "SemanticType::createInt()";
                case types::SemanticTypeKind::Long: return "SemanticType::createLong()";
                case types::SemanticTypeKind::Double: return "SemanticType::createDouble()";
                case types::SemanticTypeKind::String: return "SemanticType::createString()";
                case types::SemanticTypeKind::Bool: return "SemanticType::createBool()";
                case types::SemanticTypeKind::Json: return "SemanticType::createJson()";
                case types::SemanticTypeKind::Model:
                    return "SemanticType::createModel(" + quote(value.getModelName()) + ")";
                case types::SemanticTypeKind::List:
                    if (value.getElementType() == nullptr) {
                        throw runtime_error("Android native generation found a List type without an element type.");
                    }
                    return "SemanticType::createList(" + type(*value.getElementType()) + ")";
            }
            throw runtime_error("Android native generation does not support this semantic type.");
        }

        // Serializes one original source location without discarding provenance.
        [[nodiscard]] static string location(const source::SourceLocation& value) {
            const string_view sourcePath = value.getSourcePath();
            if (sourcePath.empty()) {
                return "SourceLocation(" + to_string(value.getLine()) + ", " +
                    to_string(value.getColumn()) + ")";
            }
            return "SourceLocation(" + quote(string(sourcePath)) + ", " +
                to_string(value.getLine()) + ", " +
                to_string(value.getColumn()) + ")";
        }

        // Serializes one complete currently-supported IR expression.
        [[nodiscard]] static string expression(const ir::IrExpression& value) {
            switch (value.getKind()) {
                case ir::IrExpressionKind::ReadSymbol: {
                    const auto& node = static_cast<const ir::IrReadSymbolExpression&>(value);
                    return "make_unique<IrReadSymbolExpression>(" + quote(node.getName()) +
                        ", IrSymbolKind::" + symbol(node.getSymbolKind()) + ", " +
                        type(node.getType()) + ", " + location(node.getLocation()) + ")";
                }
                case ir::IrExpressionKind::IntegerConstant: {
                    const auto& node = static_cast<const ir::IrIntegerConstantExpression&>(value);
                    return "make_unique<IrIntegerConstantExpression>(" + quote(node.getValue()) +
                        ", " + type(node.getType()) + ", " + location(node.getLocation()) + ")";
                }
                case ir::IrExpressionKind::DoubleConstant: {
                    const auto& node = static_cast<const ir::IrDoubleConstantExpression&>(value);
                    return "make_unique<IrDoubleConstantExpression>(" + quote(node.getValue()) +
                        ", " + location(node.getLocation()) + ")";
                }
                case ir::IrExpressionKind::StringBuild:
                    return stringBuild(static_cast<const ir::IrStringBuildExpression&>(value));
                case ir::IrExpressionKind::BooleanConstant: {
                    const auto& node = static_cast<const ir::IrBooleanConstantExpression&>(value);
                    return "make_unique<IrBooleanConstantExpression>(" +
                        string(node.getValue() ? "true" : "false") + ", " +
                        location(node.getLocation()) + ")";
                }
                case ir::IrExpressionKind::Call:
                    return call(static_cast<const ir::IrCallExpression&>(value));
                case ir::IrExpressionKind::Unary: {
                    const auto& node = static_cast<const ir::IrUnaryExpression&>(value);
                    return "make_unique<IrUnaryExpression>(IrArithmeticOperator::" +
                        arithmetic(node.getOperator()) + ", " + expression(node.getOperand()) +
                        ", " + type(node.getType()) + ", " + location(node.getLocation()) + ")";
                }
                case ir::IrExpressionKind::Binary: {
                    const auto& node = static_cast<const ir::IrBinaryExpression&>(value);
                    return "make_unique<IrBinaryExpression>(" + expression(node.getLeft()) +
                        ", IrArithmeticOperator::" + arithmetic(node.getOperator()) +
                        ", " + expression(node.getRight()) + ", " + type(node.getType()) +
                        ", " + location(node.getLocation()) + ")";
                }
                case ir::IrExpressionKind::JsonNumber: {
                    const auto& node = static_cast<const ir::IrJsonNumberExpression&>(value);
                    return "make_unique<IrJsonNumberExpression>(" + quote(node.getValue()) +
                        ", " + location(node.getLocation()) + ")";
                }
                case ir::IrExpressionKind::JsonNull:
                    return "make_unique<IrJsonNullExpression>(" + location(value.getLocation()) + ")";
                case ir::IrExpressionKind::JsonObject:
                    return jsonObject(static_cast<const ir::IrJsonObjectExpression&>(value));
                case ir::IrExpressionKind::JsonArray:
                    return jsonArray(static_cast<const ir::IrJsonArrayExpression&>(value));
                case ir::IrExpressionKind::CrossaRequest:
                    return request(static_cast<const ir::IrCrossaRequestExpression&>(value));
            }
            throw runtime_error("Android native generation does not support this IR expression.");
        }

        // Serializes one complete currently-supported IR statement.
        [[nodiscard]] static string statement(const ir::IrStatement& value) {
            switch (value.getKind()) {
                case ir::IrStatementKind::Return: {
                    const auto& node = static_cast<const ir::IrReturnStatement&>(value);
                    return "make_unique<IrReturnStatement>(" + expression(node.getExpression()) +
                        ", " + location(node.getLocation()) + ")";
                }
                case ir::IrStatementKind::Evaluate: {
                    const auto& node = static_cast<const ir::IrEvaluateStatement&>(value);
                    return "make_unique<IrEvaluateStatement>(" + expression(node.getExpression()) +
                        ", " + location(node.getLocation()) + ")";
                }
                case ir::IrStatementKind::Local: {
                    const auto& node = static_cast<const ir::IrLocalStatement&>(value);
                    return "make_unique<IrLocalStatement>(" + quote(node.getName()) + ", " +
                        type(node.getType()) + ", " + expression(node.getInitializer()) + ", " +
                        location(node.getLocation()) + ")";
                }
                case ir::IrStatementKind::If:
                    return conditional(static_cast<const ir::IrIfStatement&>(value));
            }
            throw runtime_error("Android native generation does not support this IR statement.");
        }

        // Serializes one complete currently-supported IR declaration.
        [[nodiscard]] static string declaration(const ir::IrDeclaration& value) {
            switch (value.getKind()) {
                case ir::IrDeclarationKind::Variable:
                    return variable(static_cast<const ir::IrVariableDeclaration&>(value));
                case ir::IrDeclarationKind::Model:
                    return model(static_cast<const ir::IrModelDeclaration&>(value));
                case ir::IrDeclarationKind::Function:
                    return function(static_cast<const ir::IrFunctionDeclaration&>(value));
                case ir::IrDeclarationKind::Config:
                    return config(static_cast<const ir::IrConfigDeclaration&>(value));
                case ir::IrDeclarationKind::Expression:
                    return expressionDeclaration(static_cast<const ir::IrExpressionDeclaration&>(value));
            }
            throw runtime_error("Android native generation does not support this IR declaration.");
        }

        // Returns a location-stable key for one canonical linked declaration.
        [[nodiscard]] static string declarationKey(
            const ir::IrDeclaration& declaration,
            const string& fallbackIdentity
        ) {
            const source::SourceLocation& location = declaration.getLocation();
            const string sourcePath = location.getSourcePath().empty()
                ? fallbackIdentity
                : string(location.getSourcePath());
            return sourcePath + ":" + to_string(location.getLine()) + ":" +
                to_string(location.getColumn()) + ":" + declarationKind(declaration.getKind());
        }

    private:
        // Serializes one string construction plan.
        [[nodiscard]] static string stringBuild(const ir::IrStringBuildExpression& value) {
            string output = "[] { vector<IrStringSegment> segments; ";
            for (const ir::IrStringSegment& segment : value.getSegments()) {
                output += "segments.emplace_back(IrStringSegmentKind::" +
                    string(segment.getKind() == ir::IrStringSegmentKind::Literal ? "Literal" : "Symbol") +
                    ", " + quote(segment.getValue()) + ", ";
                output += segment.getSymbolKind() == nullptr
                    ? "nullopt"
                    : "optional<IrSymbolKind>(IrSymbolKind::" + symbol(*segment.getSymbolKind()) + ")";
                output += ", " + location(segment.getLocation()) + "); ";
            }
            return output + "return make_unique<IrStringBuildExpression>(move(segments), " +
                location(value.getLocation()) + "); }()";
        }

        // Serializes one canonical function call without re-resolving its callee.
        [[nodiscard]] static string call(const ir::IrCallExpression& value) {
            string output = "[] { vector<unique_ptr<IrExpression>> arguments; ";
            for (const unique_ptr<ir::IrExpression>& argument : value.getArguments()) {
                output += "arguments.push_back(" + expression(*argument) + "); ";
            }
            return output + "return make_unique<IrCallExpression>(" + quote(value.getCallee()) +
                ", " + string(value.isBuiltin() ? "true" : "false") + ", move(arguments), " +
                type(value.getType()) + ", " + location(value.getLocation()) + "); }()";
        }

        // Serializes one JSON object construction plan.
        [[nodiscard]] static string jsonObject(const ir::IrJsonObjectExpression& value) {
            string output = "[] { vector<IrJsonObjectEntry> entries; ";
            for (const ir::IrJsonObjectEntry& entry : value.getEntries()) {
                output += "entries.emplace_back(" + quote(entry.getKey()) + ", " +
                    expression(entry.getValue()) + ", " + location(entry.getLocation()) + "); ";
            }
            return output + "return make_unique<IrJsonObjectExpression>(move(entries), " +
                location(value.getLocation()) + "); }()";
        }

        // Serializes one JSON array construction plan.
        [[nodiscard]] static string jsonArray(const ir::IrJsonArrayExpression& value) {
            string output = "[] { vector<unique_ptr<IrExpression>> values; ";
            for (const unique_ptr<ir::IrExpression>& entry : value.getValues()) {
                output += "values.push_back(" + expression(*entry) + "); ";
            }
            return output + "return make_unique<IrJsonArrayExpression>(move(values), " +
                location(value.getLocation()) + "); }()";
        }

        // Serializes one complete native CrossaRequest plan.
        [[nodiscard]] static string request(const ir::IrCrossaRequestExpression& value) {
            return "make_unique<IrCrossaRequestExpression>(IrHttpMethod::" +
                httpMethod(value.getMethod()) + ", " + expression(value.getUrl()) + ", " +
                optionalExpression(value.getHeaders()) + ", " +
                optionalExpression(value.getCustomHeaders()) + ", " +
                optionalExpression(value.getQueryParams()) + ", " +
                optionalExpression(value.getBody()) + ", " +
                optionalExpression(value.getTimeout()) + ", " +
                optionalExpression(value.getRetryPolicy()) + ", " +
                optionalExpression(value.getAuthentication()) + ", " +
                optionalExpression(value.getMultipart()) + ", " +
                optionalExpression(value.getUploadProgress()) + ", " +
                optionalExpression(value.getDownloadStreaming()) + ", " +
                optionalExpression(value.getCoalesce()) + ", " +
                optionalExpression(value.getProxy()) + ", " +
                optionalExpression(value.getCertificatePolicy()) + ", " +
                optionalExpression(value.getTelemetry()) + ", " + type(value.getType()) +
                ", " + location(value.getLocation()) + ")";
        }

        // Serializes one conditional branch instruction.
        [[nodiscard]] static string conditional(const ir::IrIfStatement& value) {
            string output = "[] { vector<unique_ptr<IrStatement>> thenStatements; ";
            for (const unique_ptr<ir::IrStatement>& statement : value.getThenStatements()) {
                output += "thenStatements.push_back(" + NativeProgramEmitter::statement(*statement) + "); ";
            }
            output += "optional<vector<unique_ptr<IrStatement>>> elseStatements; ";
            if (const vector<unique_ptr<ir::IrStatement>>* elseStatements =
                    value.getElseStatements(); elseStatements != nullptr) {
                output += "elseStatements.emplace(); ";
                for (const unique_ptr<ir::IrStatement>& statement : *elseStatements) {
                    output += "elseStatements->push_back(" + NativeProgramEmitter::statement(*statement) + "); ";
                }
            }
            return output + "return make_unique<IrIfStatement>(" + expression(value.getCondition()) +
                ", move(thenStatements), move(elseStatements), " +
                location(value.getLocation()) + "); }()";
        }

        // Serializes one source variable declaration.
        [[nodiscard]] static string variable(const ir::IrVariableDeclaration& value) {
            return "declarations.push_back(make_unique<IrVariableDeclaration>(" + quote(value.getName()) +
                ", " + type(value.getType()) + ", " + expression(value.getInitializer()) +
                ", " + location(value.getLocation()) + "));\n";
        }

        // Serializes one canonical model declaration.
        [[nodiscard]] static string model(const ir::IrModelDeclaration& value) {
            string output = "{ vector<IrModelField> fields;\n";
            for (const ir::IrModelField& field : value.getFields()) {
                output += "fields.emplace_back(" + quote(field.getName()) + ", " +
                    type(field.getType()) + ", " + location(field.getLocation()) + ");\n";
            }
            return output + "declarations.push_back(make_unique<IrModelDeclaration>(" + quote(value.getName()) +
                ", move(fields), " + location(value.getLocation()) + ")); }\n";
        }

        // Serializes one function declaration and every statement in its body.
        [[nodiscard]] static string function(const ir::IrFunctionDeclaration& value) {
            string output = "{ vector<IrParameter> parameters;\n";
            for (const ir::IrParameter& parameter : value.getParameters()) {
                output += "parameters.emplace_back(" + quote(parameter.getName()) + ", " +
                    type(parameter.getType()) + ", " + location(parameter.getLocation()) + ");\n";
            }
            output += "vector<unique_ptr<IrStatement>> statements;\n";
            for (const unique_ptr<ir::IrStatement>& statement : value.getStatements()) {
                output += "statements.push_back(" + NativeProgramEmitter::statement(*statement) + ");\n";
            }
            return output + "declarations.push_back(make_unique<IrFunctionDeclaration>(" + quote(value.getName()) +
                ", IrExecutionPolicy::" + executionPolicy(value.getExecutionPolicy()) +
                ", move(parameters), " + type(value.getReturnType()) + ", move(statements), " +
                location(value.getLocation()) + ")); }\n";
        }

        // Serializes one declarative runtime configuration block.
        [[nodiscard]] static string config(const ir::IrConfigDeclaration& value) {
            string output = "{ vector<IrConfigEntry> entries;\n";
            for (const ir::IrConfigEntry& entry : value.getEntries()) {
                output += "entries.emplace_back(" + quote(entry.getName()) + ", " +
                    type(entry.getType()) + ", " + expression(entry.getValue()) + ", " +
                    location(entry.getLocation()) + ");\n";
            }
            return output + "declarations.push_back(make_unique<IrConfigDeclaration>(move(entries), " +
                location(value.getLocation()) + ")); }\n";
        }

        // Serializes one inert top-level expression declaration.
        [[nodiscard]] static string expressionDeclaration(const ir::IrExpressionDeclaration& value) {
            return "declarations.push_back(make_unique<IrExpressionDeclaration>(" +
                expression(value.getExpression()) + ", " + location(value.getLocation()) + "));\n";
        }

        // Serializes one optional nested expression.
        [[nodiscard]] static string optionalExpression(const ir::IrExpression* value) {
            return value == nullptr ? "nullptr" : expression(*value);
        }

        // Serializes one complete arithmetic operator.
        [[nodiscard]] static string arithmetic(ir::IrArithmeticOperator value) {
            switch (value) {
                case ir::IrArithmeticOperator::Add: return "Add";
                case ir::IrArithmeticOperator::Subtract: return "Subtract";
                case ir::IrArithmeticOperator::Multiply: return "Multiply";
                case ir::IrArithmeticOperator::Divide: return "Divide";
                case ir::IrArithmeticOperator::Negate: return "Negate";
                case ir::IrArithmeticOperator::Equal: return "Equal";
                case ir::IrArithmeticOperator::NotEqual: return "NotEqual";
                case ir::IrArithmeticOperator::Less: return "Less";
                case ir::IrArithmeticOperator::LessEqual: return "LessEqual";
                case ir::IrArithmeticOperator::Greater: return "Greater";
                case ir::IrArithmeticOperator::GreaterEqual: return "GreaterEqual";
                case ir::IrArithmeticOperator::LogicalAnd: return "LogicalAnd";
                case ir::IrArithmeticOperator::LogicalOr: return "LogicalOr";
                case ir::IrArithmeticOperator::Not: return "Not";
            }
            throw runtime_error("Android native generation does not support this IR arithmetic operator.");
        }

        // Serializes one resolved symbol ownership category.
        [[nodiscard]] static string symbol(ir::IrSymbolKind value) {
            switch (value) {
                case ir::IrSymbolKind::SourceVariable: return "SourceVariable";
                case ir::IrSymbolKind::Parameter: return "Parameter";
                case ir::IrSymbolKind::LocalVariable: return "LocalVariable";
            }
            throw runtime_error("Android native generation does not support this IR symbol kind.");
        }

        // Serializes one supported native HTTP method.
        [[nodiscard]] static string httpMethod(ir::IrHttpMethod value) {
            switch (value) {
                case ir::IrHttpMethod::Get: return "Get";
                case ir::IrHttpMethod::Post: return "Post";
                case ir::IrHttpMethod::Put: return "Put";
                case ir::IrHttpMethod::Patch: return "Patch";
                case ir::IrHttpMethod::Delete: return "Delete";
                case ir::IrHttpMethod::Head: return "Head";
                case ir::IrHttpMethod::Options: return "Options";
                case ir::IrHttpMethod::Trace: return "Trace";
                case ir::IrHttpMethod::Connect: return "Connect";
            }
            throw runtime_error("Android native generation does not support this HTTP method.");
        }

        // Serializes one function execution policy.
        [[nodiscard]] static string executionPolicy(ir::IrExecutionPolicy value) {
            switch (value) {
                case ir::IrExecutionPolicy::Sync: return "Sync";
                case ir::IrExecutionPolicy::Async: return "Async";
                case ir::IrExecutionPolicy::AsyncAfter: return "AsyncAfter";
            }
            throw runtime_error("Android native generation does not support this execution policy.");
        }

        // Returns a stable textual category for one declaration kind.
        [[nodiscard]] static string declarationKind(ir::IrDeclarationKind value) {
            switch (value) {
                case ir::IrDeclarationKind::Variable: return "Variable";
                case ir::IrDeclarationKind::Model: return "Model";
                case ir::IrDeclarationKind::Function: return "Function";
                case ir::IrDeclarationKind::Config: return "Config";
                case ir::IrDeclarationKind::Expression: return "Expression";
            }
            throw runtime_error("Android native generation does not support this declaration kind.");
        }
    };

    // Generates immutable operation identifier declarations for one program.
    string NativeProgramGenerator::generateOperationHeader(const ir::Program& program) const {
        return generateOperationHeader(vector<const ir::Program*>{&program});
    }

    // Generates one project-wide immutable operation identifier declaration set.
    string NativeProgramGenerator::generateOperationHeader(
        const vector<const ir::Program*>& programs
    ) const {
        ostringstream output;
        output << "#pragma once\n\n#include <cstdint>\n\n";
        output << "namespace crossa::generated {\n\n";
        output << "class GeneratedOperations final {\npublic:\n";
        set<string> emittedFunctions;
        set<string> operationNames;
        set<uint64_t> operationIds;
        for (const ir::Program* program : programs) {
            if (program == nullptr) {
                throw runtime_error("Android native generation requires linked IR.");
            }
            for (const unique_ptr<ir::IrDeclaration>& declaration : program->getDeclarations()) {
                if (declaration->getKind() != ir::IrDeclarationKind::Function) {
                    continue;
                }
                const auto& function = static_cast<const ir::IrFunctionDeclaration&>(*declaration);
                const string key = NativeProgramEmitter::declarationKey(function, program->getIdentity());
                if (!emittedFunctions.insert(key).second) {
                    continue;
                }
                if (!operationNames.insert(function.getName()).second) {
                    throw runtime_error("Android native generation found duplicate operation symbol '" +
                        function.getName() + "'.");
                }
                const uint64_t id = operationId(sourceIdentity(*program, function), function);
                if (!operationIds.insert(id).second) {
                    throw runtime_error("Android native generation found a generated operation ID collision.");
                }
                output << "    static constexpr std::uint64_t " << function.getName() << " = "
                       << id << "ULL;\n";
            }
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
        return generateProgramSource(vector<const ir::Program*>{&program});
    }

    // Generates one native program reconstruction from every linked project source.
    string NativeProgramGenerator::generateProgramSource(
        const vector<const ir::Program*>& programs
    ) const {
        ostringstream output;
        output << "#include \"CrossaGeneratedProgram.h\"\n"
               << "#include <memory>\n"
               << "#include <optional>\n"
               << "#include <vector>\n"
               << "#include <crossa/compiler/ir/IrCrossaRequestExpression.h>\n"
               << "#include <crossa/compiler/ir/IrDeclaration.h>\n"
               << "#include <crossa/compiler/ir/IrJsonExpression.h>\n"
               << "#include <crossa/compiler/source/SourceLocation.h>\n\n"
               << "using namespace std;\n"
               << "namespace crossa::generated {\n"
               << "compiler::ir::Program CrossaGeneratedProgram::create() {\n"
               << "using namespace compiler::ir;\n"
               << "using namespace compiler::types;\n"
               << "using compiler::source::SourceLocation;\n"
               << "vector<unique_ptr<IrDeclaration>> declarations;\n";
        set<string> emittedDeclarations;
        map<string, string> modelKeys;
        map<string, string> functionKeys;
        for (const ir::Program* program : programs) {
            if (program == nullptr) {
                throw runtime_error("Android native generation requires linked IR.");
            }
            for (const unique_ptr<ir::IrDeclaration>& declaration : program->getDeclarations()) {
                const string key = NativeProgramEmitter::declarationKey(
                    *declaration,
                    program->getIdentity()
                );
                if (declaration->getKind() == ir::IrDeclarationKind::Model) {
                    const auto& model = static_cast<const ir::IrModelDeclaration&>(*declaration);
                    const auto existing = modelKeys.find(model.getName());
                    if (existing != modelKeys.end() && existing->second != key) {
                        throw runtime_error("Android native generation found duplicate model symbol '" +
                            model.getName() + "'.");
                    }
                    modelKeys.emplace(model.getName(), key);
                }
                if (declaration->getKind() == ir::IrDeclarationKind::Function) {
                    const auto& function = static_cast<const ir::IrFunctionDeclaration&>(*declaration);
                    const auto existing = functionKeys.find(function.getName());
                    if (existing != functionKeys.end() && existing->second != key) {
                        throw runtime_error("Android native generation found duplicate function symbol '" +
                            function.getName() + "'.");
                    }
                    functionKeys.emplace(function.getName(), key);
                }
                if (!emittedDeclarations.insert(key).second) {
                    continue;
                }
                output << NativeProgramEmitter::declaration(*declaration);
            }
        }
        output << "return compiler::ir::Program({}, \"CrossaGenerated\", std::move(declarations));\n"
               << "}\n"
               << "}\n";
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

    // Returns the original source-unit identity that owns one generated function.
    string NativeProgramGenerator::sourceIdentity(
        const ir::Program& program,
        const ir::IrFunctionDeclaration& function
    ) {
        const string_view sourcePath = function.getLocation().getSourcePath();
        if (sourcePath.empty()) {
            return program.getIdentity();
        }
        return filesystem::path(string(sourcePath)).stem().string();
    }

}
