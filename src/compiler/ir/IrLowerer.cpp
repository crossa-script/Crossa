#include "crossa/compiler/ir/IrLowerer.h"

#include <stdexcept>
#include <utility>

using namespace std;

namespace crossa::compiler::ir {

    // Builds a complete IR program from one typed semantic source unit.
    Program IrLowerer::lower(const semantic::TypedSourceUnit& sourceUnit) {
        vector<unique_ptr<IrDeclaration>> declarations;
        declarations.reserve(sourceUnit.getDeclarations().size());
        for (const unique_ptr<semantic::TypedDeclaration>& declaration :
             sourceUnit.getDeclarations()) {
            declarations.push_back(lowerDeclaration(*declaration));
        }

        return Program(
            sourceUnit.getSourcePath(),
            sourceUnit.getIdentity(),
            std::move(declarations)
        );
    }

    // Lowers one typed declaration into a platform-neutral IR declaration.
    unique_ptr<IrDeclaration> IrLowerer::lowerDeclaration(
        const semantic::TypedDeclaration& declaration
    ) {
        switch (declaration.getKind()) {
            case semantic::TypedDeclarationKind::Variable:
                return lowerVariableDeclaration(
                    static_cast<const semantic::TypedVariableDeclaration&>(
                        declaration
                    )
                );
            case semantic::TypedDeclarationKind::Model:
                return lowerModelDeclaration(
                    static_cast<const semantic::TypedModelDeclaration&>(
                        declaration
                    )
                );
            case semantic::TypedDeclarationKind::Function:
                return lowerFunctionDeclaration(
                    static_cast<const semantic::TypedFunctionDeclaration&>(
                        declaration
                    )
                );
            case semantic::TypedDeclarationKind::Config:
                return lowerConfigDeclaration(
                    static_cast<const semantic::TypedConfigDeclaration&>(
                        declaration
                    )
                );
        }

        fail("Unknown typed declaration kind.");
    }

    // Lowers one typed source variable declaration.
    unique_ptr<IrDeclaration> IrLowerer::lowerVariableDeclaration(
        const semantic::TypedVariableDeclaration& declaration
    ) {
        return make_unique<IrVariableDeclaration>(
            declaration.getName(),
            declaration.getType(),
            lowerExpression(declaration.getInitializer()),
            declaration.getLocation()
        );
    }

    // Lowers one typed model declaration and its ordered fields.
    unique_ptr<IrDeclaration> IrLowerer::lowerModelDeclaration(
        const semantic::TypedModelDeclaration& declaration
    ) {
        vector<IrModelField> fields;
        fields.reserve(declaration.getFields().size());
        for (const semantic::TypedModelField& field : declaration.getFields()) {
            fields.emplace_back(
                field.getName(),
                field.getType(),
                field.getLocation()
            );
        }

        return make_unique<IrModelDeclaration>(
            declaration.getName(),
            std::move(fields),
            declaration.getLocation()
        );
    }

    // Lowers one typed function declaration and its body instructions.
    unique_ptr<IrDeclaration> IrLowerer::lowerFunctionDeclaration(
        const semantic::TypedFunctionDeclaration& declaration
    ) {
        vector<IrParameter> parameters;
        parameters.reserve(declaration.getParameters().size());
        for (const semantic::TypedParameter& parameter :
             declaration.getParameters()) {
            parameters.emplace_back(
                parameter.getName(),
                parameter.getType(),
                parameter.getLocation()
            );
        }

        vector<unique_ptr<IrStatement>> statements;
        statements.reserve(declaration.getStatements().size());
        for (const unique_ptr<semantic::TypedStatement>& statement :
             declaration.getStatements()) {
            statements.push_back(lowerStatement(*statement));
        }

        return make_unique<IrFunctionDeclaration>(
            declaration.getName(),
            lowerExecutionPolicy(declaration.getExecutionPolicy()),
            std::move(parameters),
            declaration.getReturnType(),
            std::move(statements),
            declaration.getLocation()
        );
    }

    // Lowers one typed configuration declaration.
    unique_ptr<IrDeclaration> IrLowerer::lowerConfigDeclaration(
        const semantic::TypedConfigDeclaration& declaration
    ) {
        vector<IrConfigEntry> entries;
        entries.reserve(declaration.getEntries().size());
        for (const semantic::TypedConfigEntry& entry : declaration.getEntries()) {
            entries.emplace_back(
                entry.getName(),
                entry.getType(),
                lowerExpression(entry.getValue()),
                entry.getLocation()
            );
        }

        return make_unique<IrConfigDeclaration>(
            std::move(entries),
            declaration.getLocation()
        );
    }

    // Lowers one typed statement into an IR instruction.
    unique_ptr<IrStatement> IrLowerer::lowerStatement(
        const semantic::TypedStatement& statement
    ) {
        switch (statement.getKind()) {
            case semantic::TypedStatementKind::Return: {
                const auto& returnStatement =
                    static_cast<const semantic::TypedReturnStatement&>(statement);
                return make_unique<IrReturnStatement>(
                    lowerExpression(returnStatement.getExpression()),
                    statement.getLocation()
                );
            }
            case semantic::TypedStatementKind::Expression: {
                const auto& expressionStatement =
                    static_cast<const semantic::TypedExpressionStatement&>(
                        statement
                    );
                return make_unique<IrEvaluateStatement>(
                    lowerExpression(expressionStatement.getExpression()),
                    statement.getLocation()
                );
            }
            case semantic::TypedStatementKind::Variable: {
                const auto& variable =
                    static_cast<const semantic::TypedVariableStatement&>(
                        statement
                    );
                return make_unique<IrLocalStatement>(
                    variable.getName(),
                    variable.getType(),
                    lowerExpression(variable.getInitializer()),
                    statement.getLocation()
                );
            }
        }

        fail("Unknown typed statement kind.");
    }

    // Lowers one typed expression recursively into IR instructions.
    unique_ptr<IrExpression> IrLowerer::lowerExpression(
        const semantic::TypedExpression& expression
    ) {
        switch (expression.getKind()) {
            case semantic::TypedExpressionKind::Identifier: {
                const auto& identifier =
                    static_cast<const semantic::TypedIdentifierExpression&>(
                        expression
                    );
                return make_unique<IrReadSymbolExpression>(
                    identifier.getName(),
                    lowerSymbolKind(identifier.getSymbolKind()),
                    identifier.getType(),
                    identifier.getLocation()
                );
            }
            case semantic::TypedExpressionKind::IntegerLiteral: {
                const auto& literal =
                    static_cast<
                        const semantic::TypedIntegerLiteralExpression&
                    >(expression);
                return make_unique<IrIntegerConstantExpression>(
                    literal.getValue(),
                    literal.getLocation()
                );
            }
            case semantic::TypedExpressionKind::StringLiteral: {
                const auto& stringLiteral =
                    static_cast<const semantic::TypedStringLiteralExpression&>(
                        expression
                    );
                vector<IrStringSegment> segments;
                segments.reserve(stringLiteral.getSegments().size());
                for (const semantic::TypedStringSegment& segment :
                     stringLiteral.getSegments()) {
                    optional<IrSymbolKind> symbolKind;
                    if (const semantic::ValueSymbolKind* kind =
                            segment.getSymbolKind();
                        kind != nullptr) {
                        symbolKind = lowerSymbolKind(*kind);
                    }
                    segments.emplace_back(
                        segment.getKind() ==
                                semantic::TypedStringSegmentKind::Literal
                            ? IrStringSegmentKind::Literal
                            : IrStringSegmentKind::Symbol,
                        segment.getValue(),
                        symbolKind,
                        segment.getLocation()
                    );
                }
                return make_unique<IrStringBuildExpression>(
                    std::move(segments),
                    stringLiteral.getLocation()
                );
            }
            case semantic::TypedExpressionKind::BooleanLiteral: {
                const auto& literal =
                    static_cast<const semantic::TypedBooleanLiteralExpression&>(
                        expression
                    );
                return make_unique<IrBooleanConstantExpression>(
                    literal.getValue(),
                    literal.getLocation()
                );
            }
            case semantic::TypedExpressionKind::Call: {
                const auto& call =
                    static_cast<const semantic::TypedCallExpression&>(
                        expression
                    );
                vector<unique_ptr<IrExpression>> arguments;
                arguments.reserve(call.getArguments().size());
                for (const unique_ptr<semantic::TypedExpression>& argument :
                     call.getArguments()) {
                    arguments.push_back(lowerExpression(*argument));
                }
                return make_unique<IrCallExpression>(
                    call.getCallee(),
                    call.isBuiltin(),
                    std::move(arguments),
                    call.getType(),
                    call.getLocation()
                );
            }
            case semantic::TypedExpressionKind::Unary: {
                const auto& unary =
                    static_cast<const semantic::TypedUnaryExpression&>(
                        expression
                    );
                return make_unique<IrUnaryExpression>(
                    lowerUnaryOperator(unary.getOperator()),
                    lowerExpression(unary.getOperand()),
                    unary.getType(),
                    unary.getLocation()
                );
            }
            case semantic::TypedExpressionKind::Binary: {
                const auto& binary =
                    static_cast<const semantic::TypedBinaryExpression&>(
                        expression
                    );
                return make_unique<IrBinaryExpression>(
                    lowerExpression(binary.getLeft()),
                    lowerBinaryOperator(binary.getOperator()),
                    lowerExpression(binary.getRight()),
                    binary.getType(),
                    binary.getLocation()
                );
            }
        }

        fail("Unknown typed expression kind.");
    }

    // Maps a semantic symbol owner to its IR symbol owner.
    IrSymbolKind IrLowerer::lowerSymbolKind(
        semantic::ValueSymbolKind symbolKind
    ) noexcept {
        switch (symbolKind) {
            case semantic::ValueSymbolKind::SourceVariable:
                return IrSymbolKind::SourceVariable;
            case semantic::ValueSymbolKind::Parameter:
                return IrSymbolKind::Parameter;
            case semantic::ValueSymbolKind::LocalVariable:
                return IrSymbolKind::LocalVariable;
        }

        return IrSymbolKind::SourceVariable;
    }

    // Maps a semantic execution policy to its IR scheduling policy.
    IrExecutionPolicy IrLowerer::lowerExecutionPolicy(
        semantic::SemanticExecutionPolicy executionPolicy
    ) noexcept {
        switch (executionPolicy) {
            case semantic::SemanticExecutionPolicy::Sync:
                return IrExecutionPolicy::Sync;
            case semantic::SemanticExecutionPolicy::Async:
                return IrExecutionPolicy::Async;
            case semantic::SemanticExecutionPolicy::AsyncAfter:
                return IrExecutionPolicy::AsyncAfter;
        }

        return IrExecutionPolicy::Sync;
    }

    // Maps a typed unary operation to an IR arithmetic operation.
    IrArithmeticOperator IrLowerer::lowerUnaryOperator(
        semantic::TypedUnaryOperator operation
    ) noexcept {
        switch (operation) {
            case semantic::TypedUnaryOperator::Negate:
                return IrArithmeticOperator::Negate;
        }

        return IrArithmeticOperator::Negate;
    }

    // Maps a typed binary operation to an IR arithmetic operation.
    IrArithmeticOperator IrLowerer::lowerBinaryOperator(
        semantic::TypedBinaryOperator operation
    ) noexcept {
        switch (operation) {
            case semantic::TypedBinaryOperator::Add:
                return IrArithmeticOperator::Add;
            case semantic::TypedBinaryOperator::Subtract:
                return IrArithmeticOperator::Subtract;
            case semantic::TypedBinaryOperator::Multiply:
                return IrArithmeticOperator::Multiply;
            case semantic::TypedBinaryOperator::Divide:
                return IrArithmeticOperator::Divide;
        }

        return IrArithmeticOperator::Add;
    }

    // Raises an internal error if a validated node cannot be lowered.
    [[noreturn]] void IrLowerer::fail(const char* message) {
        throw runtime_error(string("IR lowering invariant failed: ") + message);
    }

}
