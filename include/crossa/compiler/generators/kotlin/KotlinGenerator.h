#pragma once

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "crossa/compiler/generators/kotlin/KotlinGeneratedSource.h"
#include "crossa/compiler/generators/kotlin/KotlinIdentifierEscaper.h"
#include "crossa/compiler/generators/kotlin/KotlinTypeMapper.h"
#include "crossa/compiler/ir/IrCrossaRequestExpression.h"
#include "crossa/compiler/ir/IrExpression.h"
#include "crossa/compiler/ir/IrStatement.h"
#include "crossa/compiler/ir/Program.h"

namespace crossa::compiler::generators::kotlin {

// Generates one deterministic pure Kotlin source unit from platform-neutral IR.
class KotlinGenerator final {
public:
    // Generates the Kotlin file identity and canonical source for one IR program.
    [[nodiscard]] KotlinGeneratedSource generate(
        const ir::Program& program,
        const std::optional<std::string>& packageName = std::nullopt
    ) const;

private:
    using ModelMap = std::unordered_map<std::string, const ir::IrModelDeclaration*>;

    void emitModel(
        const ir::IrModelDeclaration& model,
        class KotlinSourceWriter& writer
    ) const;

    // Emits one pure synchronous Kotlin function.
    void emitFunction(
        const ir::IrFunctionDeclaration& function,
        const ModelMap& models,
        class KotlinSourceWriter& writer
    ) const;

    void emitAsyncAfterRequestFunction(
        const ir::IrFunctionDeclaration& function,
        const ModelMap& models,
        class KotlinSourceWriter& writer
    ) const;

    void emitRequestBlockingFunction(
        const ir::IrFunctionDeclaration& function,
        const ir::IrCrossaRequestExpression& request,
        const ModelMap& models,
        class KotlinSourceWriter& writer
    ) const;

    void emitModelMapper(
        const ir::IrModelDeclaration& model,
        class KotlinSourceWriter& writer
    ) const;

    [[nodiscard]] static const ir::IrCrossaRequestExpression*
    findReturnedRequest(const ir::IrFunctionDeclaration& function);

    [[nodiscard]] static std::string readStringLiteral(
        const ir::IrExpression& expression
    );

    [[nodiscard]] static std::vector<std::pair<std::string, std::string>>
    readStringMap(const ir::IrExpression* expression);

    [[nodiscard]] static std::string escapeKotlinStringLiteral(
        const std::string& value
    );

    // Emits one Kotlin function signature using the canonical wrapping policy.
    void emitFunctionSignature(
        const ir::IrFunctionDeclaration& function,
        class KotlinSourceWriter& writer
    ) const;

    // Returns the deterministic Kotlin file name for one source unit.
    [[nodiscard]] std::string getFileName(const ir::Program& program) const;

    // Writes the optional validated Kotlin package directive for one source file.
    static void emitPackageDirective(
        const std::optional<std::string>& packageName,
        class KotlinSourceWriter& writer
    );

    // Returns whether one configured package name is valid Kotlin package syntax.
    [[nodiscard]] static bool isValidPackageName(
        std::string_view packageName
    ) noexcept;

    // Returns whether one package segment is a valid ASCII Kotlin identifier.
    [[nodiscard]] static bool isValidPackageSegment(
        std::string_view segment
    ) noexcept;

    // Rejects runtime-backed expressions before Kotlin source emission begins.
    static void validatePureStatements(
        const std::vector<std::unique_ptr<ir::IrStatement>>& statements
    );

    // Rejects runtime-backed expressions inside one typed IR statement.
    static void validatePureStatement(const ir::IrStatement& statement);

    // Rejects runtime-backed expressions inside one typed IR expression tree.
    static void validatePureExpression(const ir::IrExpression& expression);

    // Raises a deterministic unsupported-backend diagnostic for one IR feature.
    [[noreturn]] static void failUnsupported(const char* feature);

    // Raises a deterministic internal invariant diagnostic for malformed IR.
    [[noreturn]] static void failInvariant(const char* message);

    KotlinIdentifierEscaper identifierEscaper_;
    KotlinTypeMapper typeMapper_;
};

}
