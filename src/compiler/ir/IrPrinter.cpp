#include "crossa/compiler/ir/IrPrinter.h"

#include <iterator>
#include <utility>

using namespace std;

namespace crossa::compiler::ir {

    // Returns one or more readable lines for the complete IR program.
    vector<string> IrPrinter::summarize(const Program& program) {
        vector<string> summaries;
        for (const unique_ptr<IrDeclaration>& declaration :
             program.getDeclarations()) {
            vector<string> declarationLines = summarizeDeclaration(*declaration);
            summaries.insert(
                summaries.end(),
                std::make_move_iterator(declarationLines.begin()),
                std::make_move_iterator(declarationLines.end())
            );
        }
        return summaries;
    }

    // Formats one IR declaration and its nested instructions.
    vector<string> IrPrinter::summarizeDeclaration(
        const IrDeclaration& declaration
    ) {
        vector<string> summaries;
        switch (declaration.getKind()) {
            case IrDeclarationKind::Variable: {
                const auto& variable =
                    static_cast<const IrVariableDeclaration&>(declaration);
                summaries.push_back(
                    "IR Variable " + variable.getName() + ": " +
                    variable.getType().format() + " = " +
                    formatExpression(variable.getInitializer())
                );
                break;
            }
            case IrDeclarationKind::Model: {
                const auto& model =
                    static_cast<const IrModelDeclaration&>(declaration);
                summaries.push_back(
                    "IR Model " + model.getName() + " fields=" +
                    to_string(model.getFields().size())
                );
                for (const IrModelField& field : model.getFields()) {
                    summaries.push_back(
                        "  IR Field " + field.getName() + ": " +
                        field.getType().format()
                    );
                }
                break;
            }
            case IrDeclarationKind::Function: {
                const auto& function =
                    static_cast<const IrFunctionDeclaration&>(declaration);
                summaries.push_back(
                    "IR Function " + function.getName() + " policy=" +
                    formatExecutionPolicy(function.getExecutionPolicy()) +
                    " parameters=" + to_string(function.getParameters().size()) +
                    " return=" + function.getReturnType().format()
                );
                for (const unique_ptr<IrStatement>& statement :
                     function.getStatements()) {
                    summaries.push_back(formatStatement(*statement, "  "));
                }
                break;
            }
            case IrDeclarationKind::Config: {
                const auto& config =
                    static_cast<const IrConfigDeclaration&>(declaration);
                summaries.push_back(
                    "IR Config entries=" +
                    to_string(config.getEntries().size())
                );
                for (const IrConfigEntry& entry : config.getEntries()) {
                    summaries.push_back(
                        "  IR ConfigEntry " + entry.getName() + ": " +
                        entry.getType().format() + " = " +
                        formatExpression(entry.getValue())
                    );
                }
                break;
            }
            case IrDeclarationKind::Expression: {
                const auto& expression =
                    static_cast<const IrExpressionDeclaration&>(declaration);
                summaries.push_back(
                    "IR Top-level Evaluate " +
                    formatExpression(expression.getExpression())
                );
                break;
            }
        }
        return summaries;
    }

    // Formats one IR statement at the requested indentation level.
    string IrPrinter::formatStatement(
        const IrStatement& statement,
        const string& indentation
    ) {
        switch (statement.getKind()) {
            case IrStatementKind::Return:
                return indentation + "IR Return " + formatExpression(
                    static_cast<const IrReturnStatement&>(statement).getExpression()
                );
            case IrStatementKind::Evaluate:
                return indentation + "IR Evaluate " + formatExpression(
                    static_cast<const IrEvaluateStatement&>(statement)
                        .getExpression()
                );
            case IrStatementKind::Local: {
                const auto& local = static_cast<const IrLocalStatement&>(statement);
                return indentation + "IR Local " + local.getName() + ": " +
                       local.getType().format() + " = " +
                       formatExpression(local.getInitializer());
            }
        }

        return indentation + "IR Unknown statement";
    }

    // Formats one IR expression as a compact instruction expression.
    string IrPrinter::formatExpression(const IrExpression& expression) {
        switch (expression.getKind()) {
            case IrExpressionKind::ReadSymbol: {
                const auto& read =
                    static_cast<const IrReadSymbolExpression&>(expression);
                return "Read(" + read.getName() + ")";
            }
            case IrExpressionKind::IntegerConstant:
                return static_cast<const IrIntegerConstantExpression&>(expression)
                    .getValue();
            case IrExpressionKind::StringBuild: {
                const auto& stringBuild =
                    static_cast<const IrStringBuildExpression&>(expression);
                string result = "StringBuild(";
                bool first = true;
                for (const IrStringSegment& segment : stringBuild.getSegments()) {
                    if (!first) {
                        result += ", ";
                    }
                    first = false;
                    result += segment.getKind() == IrStringSegmentKind::Literal
                        ? "Literal(\"" + segment.getValue() + "\")"
                        : "Read(" + segment.getValue() + ")";
                }
                return result + ")";
            }
            case IrExpressionKind::BooleanConstant:
                return static_cast<
                    const IrBooleanConstantExpression&
                >(expression).getValue() ? "true" : "false";
            case IrExpressionKind::Call: {
                const auto& call =
                    static_cast<const IrCallExpression&>(expression);
                string result = "Call(" + call.getCallee() + "";
                for (const unique_ptr<IrExpression>& argument :
                     call.getArguments()) {
                    result += ", " + formatExpression(*argument);
                }
                return result + ")";
            }
            case IrExpressionKind::Unary: {
                const auto& unary =
                    static_cast<const IrUnaryExpression&>(expression);
                return formatArithmeticOperator(unary.getOperator()) + "(" +
                       formatExpression(unary.getOperand()) + ")";
            }
            case IrExpressionKind::Binary: {
                const auto& binary =
                    static_cast<const IrBinaryExpression&>(expression);
                return formatArithmeticOperator(binary.getOperator()) + "(" +
                       formatExpression(binary.getLeft()) + ", " +
                       formatExpression(binary.getRight()) + ")";
            }
        }

        return "Unknown";
    }

    // Formats one IR scheduling policy.
    string IrPrinter::formatExecutionPolicy(IrExecutionPolicy executionPolicy) {
        switch (executionPolicy) {
            case IrExecutionPolicy::Sync:
                return "Sync";
            case IrExecutionPolicy::Async:
                return "Async";
            case IrExecutionPolicy::AsyncAfter:
                return "AsyncAfter";
        }
        return "Unknown";
    }

    // Formats one IR arithmetic operation.
    string IrPrinter::formatArithmeticOperator(IrArithmeticOperator operation) {
        switch (operation) {
            case IrArithmeticOperator::Add:
                return "Add";
            case IrArithmeticOperator::Subtract:
                return "Subtract";
            case IrArithmeticOperator::Multiply:
                return "Multiply";
            case IrArithmeticOperator::Divide:
                return "Divide";
            case IrArithmeticOperator::Negate:
                return "Negate";
        }
        return "Unknown";
    }

}
