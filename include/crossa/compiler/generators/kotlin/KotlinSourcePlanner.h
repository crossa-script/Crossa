#pragma once

#include <string>
#include <vector>

#include "crossa/compiler/generators/kotlin/KotlinProjectGenerationContext.h"

namespace crossa::compiler::generators::kotlin {

// Describes one Kotlin file before source emission begins.
class KotlinSourcePlan final {
public:
    // Creates one planned Kotlin output with its package and owned declarations.
    KotlinSourcePlan(
        std::string relativePath,
        std::string packageName,
        const ir::IrModelDeclaration* model,
        std::vector<const ir::IrFunctionDeclaration*> functions
    );

    // Returns the deterministic output path relative to the Kotlin package root.
    [[nodiscard]] const std::string& getRelativePath() const noexcept;

    // Returns the Kotlin package assigned to this output file.
    [[nodiscard]] const std::string& getPackageName() const noexcept;

    // Returns the canonical model emitted by this file or null for API files.
    [[nodiscard]] const ir::IrModelDeclaration* getModel() const noexcept;

    // Returns functions owned by this API file in source order.
    [[nodiscard]] const std::vector<const ir::IrFunctionDeclaration*>&
    getFunctions() const noexcept;

private:
    std::string relativePath_;
    std::string packageName_;
    const ir::IrModelDeclaration* model_;
    std::vector<const ir::IrFunctionDeclaration*> functions_;
};

// Plans deterministic model and source-unit Kotlin files for one linked project.
class KotlinSourcePlanner final {
public:
    // Creates the full Kotlin file plan before text emission starts.
    [[nodiscard]] std::vector<KotlinSourcePlan> plan(
        const KotlinProjectGenerationContext& context,
        const std::string& basePackageName
    ) const;
};

}
