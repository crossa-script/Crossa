#include "crossa/compiler/generators/kotlin/KotlinSourcePlanner.h"

#include <filesystem>
#include <set>
#include <stdexcept>
#include <utility>

using namespace std;

namespace crossa::compiler::generators::kotlin {

    // Creates one planned Kotlin output with its package and owned declarations.
    KotlinSourcePlan::KotlinSourcePlan(
        string relativePath,
        string packageName,
        string sourceIdentity,
        const ir::IrModelDeclaration* model,
        vector<const ir::IrFunctionDeclaration*> functions
    )
        : relativePath_(std::move(relativePath)),
          packageName_(std::move(packageName)),
          sourceIdentity_(std::move(sourceIdentity)),
          model_(model),
          functions_(std::move(functions)) {}

    // Returns the deterministic output path relative to the Kotlin package root.
    const string& KotlinSourcePlan::getRelativePath() const noexcept {
        return relativePath_;
    }

    // Returns the Kotlin package assigned to this output file.
    const string& KotlinSourcePlan::getPackageName() const noexcept {
        return packageName_;
    }

    // Returns the original Crossa source identity used for native operation IDs.
    const string& KotlinSourcePlan::getSourceIdentity() const noexcept {
        return sourceIdentity_;
    }

    // Returns the canonical model emitted by this file or null for API files.
    const ir::IrModelDeclaration* KotlinSourcePlan::getModel() const noexcept {
        return model_;
    }

    // Returns functions owned by this API file in source order.
    const vector<const ir::IrFunctionDeclaration*>&
    KotlinSourcePlan::getFunctions() const noexcept {
        return functions_;
    }

    // Creates the full Kotlin file plan before text emission starts.
    vector<KotlinSourcePlan> KotlinSourcePlanner::plan(
        const KotlinProjectGenerationContext& context,
        const string& basePackageName
    ) const {
        vector<KotlinSourcePlan> plans;
        set<string> outputPaths;
        for (const auto& entry : context.getModels()) {
            const string path = "model/" + entry.first + ".kt";
            if (!outputPaths.insert(path).second) {
                throw runtime_error("Kotlin generation planned duplicate output '" + path + "'.");
            }
            plans.emplace_back(
                path,
                basePackageName + ".model",
                "",
                entry.second,
                vector<const ir::IrFunctionDeclaration*>()
            );
        }
        for (const auto& entry : context.getFunctionsBySourceUnit()) {
            if (entry.second.empty()) {
                continue;
            }
            const string sourceIdentity = filesystem::path(entry.first).stem().string();
            const string className = toPascalCase(sourceIdentity);
            const string path = "api/" + className + ".kt";
            if (!outputPaths.insert(path).second) {
                throw runtime_error(
                    "Kotlin generation found conflicting PascalCase source-unit API "
                    "identity '" + className + "'."
                );
            }
            plans.emplace_back(
                path,
                basePackageName + ".api",
                sourceIdentity,
                nullptr,
                entry.second
            );
        }
        return plans;
    }

    // Converts one source identity into a deterministic PascalCase Kotlin type name.
    string KotlinSourcePlanner::toPascalCase(string_view sourceIdentity) {
        string result;
        bool capitalizeNext = true;
        for (const char character : sourceIdentity) {
            const bool isLetter = (character >= 'A' && character <= 'Z') ||
                (character >= 'a' && character <= 'z');
            const bool isDigit = character >= '0' && character <= '9';
            if (!isLetter && !isDigit) {
                capitalizeNext = true;
                continue;
            }
            if (result.empty() && isDigit) {
                result = "Crossa";
            }
            result += capitalizeNext && character >= 'a' && character <= 'z'
                ? static_cast<char>(character - ('a' - 'A'))
                : character;
            capitalizeNext = false;
        }
        if (result.empty()) {
            throw runtime_error(
                "Kotlin generation cannot derive a class name from source identity '" +
                string(sourceIdentity) + "'."
            );
        }
        return result;
    }

}
