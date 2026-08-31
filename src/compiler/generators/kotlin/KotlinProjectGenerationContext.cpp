#include "crossa/compiler/generators/kotlin/KotlinProjectGenerationContext.h"

#include <filesystem>
#include <stdexcept>

using namespace std;

namespace crossa::compiler::generators::kotlin {

    // Builds one indexed view over every linked program selected for generation.
    KotlinProjectGenerationContext::KotlinProjectGenerationContext(
        const vector<const ir::Program*>& programs
    ) {
        if (programs.empty()) {
            throw runtime_error("Kotlin project generation requires linked IR.");
        }
        for (const ir::Program* program : programs) {
            if (program == nullptr) {
                throw runtime_error("Kotlin project generation received null IR.");
            }
            indexProgram(*program);
        }
    }

    // Returns every canonical model indexed by semantic declaration name.
    const map<string, const ir::IrModelDeclaration*>&
    KotlinProjectGenerationContext::getModels() const noexcept {
        return models_;
    }

    // Returns source-owned functions ordered by deterministic source identity.
    const map<string, vector<const ir::IrFunctionDeclaration*>>&
    KotlinProjectGenerationContext::getFunctionsBySourceUnit() const noexcept {
        return functionsBySourceUnit_;
    }

    // Returns the source-unit identity owning a declaration.
    string KotlinProjectGenerationContext::sourceUnitIdentity(
        const ir::IrDeclaration& declaration
    ) const {
        return sourceIdentity(declaration, "CrossaGenerated");
    }

    // Indexes a single linked program while removing repeated imported declarations.
    void KotlinProjectGenerationContext::indexProgram(const ir::Program& program) {
        for (const unique_ptr<ir::IrDeclaration>& declaration :
             program.getDeclarations()) {
            const string sourceUnit = sourceUnitKey(
                *declaration,
                program.getIdentity()
            );
            if (declaration->getKind() == ir::IrDeclarationKind::Model) {
                registerModel(
                    static_cast<const ir::IrModelDeclaration&>(*declaration),
                    sourceUnit
                );
            } else if (declaration->getKind() ==
                       ir::IrDeclarationKind::Function) {
                registerFunction(
                    static_cast<const ir::IrFunctionDeclaration&>(*declaration),
                    sourceUnit
                );
            }
        }
    }

    // Registers one canonical model and rejects conflicting semantic ownership.
    void KotlinProjectGenerationContext::registerModel(
        const ir::IrModelDeclaration& model,
        const string& sourceUnit
    ) {
        const string key = declarationKey(model, model.getName());
        const auto existing = models_.find(model.getName());
        if (existing == models_.end()) {
            models_.emplace(model.getName(), &model);
            modelKeys_.emplace(model.getName(), key);
            return;
        }
        if (modelKeys_.at(model.getName()) != key) {
            throw runtime_error(
                "Kotlin generation found duplicate model symbol '" +
                model.getName() + "' in source unit '" + sourceUnit + "'."
            );
        }
    }

    // Registers one function owned by its original Crossa source unit.
    void KotlinProjectGenerationContext::registerFunction(
        const ir::IrFunctionDeclaration& function,
        const string& sourceUnit
    ) {
        const string key = declarationKey(
            function,
            function.getName() + ":" + function.getReturnType().format()
        );
        const auto existing = functionKeys_.find(key);
        if (existing != functionKeys_.end()) {
            return;
        }
        const auto existingSymbol = functionSymbolKeys_.find(
            function.getName()
        );
        if (existingSymbol != functionSymbolKeys_.end()) {
            throw runtime_error(
                "Kotlin generation found duplicate function symbol '" +
                function.getName() + "' in source unit '" + sourceUnit + "'."
            );
        }
        functionKeys_.emplace(key, key);
        functionSymbolKeys_.emplace(function.getName(), key);
        functionsBySourceUnit_[sourceUnit].push_back(&function);
    }

    // Returns one deterministic declaration key without pointer identity.
    string KotlinProjectGenerationContext::declarationKey(
        const ir::IrDeclaration& declaration,
        const string& name
    ) {
        const source::SourceLocation& location =
            declaration.getLocation();
        return string(location.getSourcePath()) + ":" +
            to_string(location.getLine()) + ":" +
            to_string(location.getColumn()) + ":" + name;
    }

    // Returns a source identity from provenance or falls back to Program identity.
    string KotlinProjectGenerationContext::sourceIdentity(
        const ir::IrDeclaration& declaration,
        const string& fallbackIdentity
    ) {
        const string_view sourcePath = declaration.getLocation().getSourcePath();
        if (sourcePath.empty()) {
            return fallbackIdentity;
        }
        const string identity = filesystem::path(string(sourcePath)).stem().string();
        if (identity.empty()) {
            throw runtime_error("Kotlin generation encountered an empty source identity.");
        }
        return identity;
    }

    // Returns a path-stable source-unit key for collision-safe file planning.
    string KotlinProjectGenerationContext::sourceUnitKey(
        const ir::IrDeclaration& declaration,
        const string& fallbackIdentity
    ) {
        const string_view sourcePath = declaration.getLocation().getSourcePath();
        return sourcePath.empty() ? fallbackIdentity : string(sourcePath);
    }

}
