#pragma once

#include <map>
#include <string>
#include <vector>

#include "crossa/compiler/ir/Program.h"

namespace crossa::compiler::generators::kotlin {

// Holds the immutable linked-project view consumed by Kotlin source planning.
class KotlinProjectGenerationContext final {
public:
    // Builds one indexed view over every linked program selected for generation.
    explicit KotlinProjectGenerationContext(
        const std::vector<const ir::Program*>& programs
    );

    // Returns every canonical model indexed by semantic declaration name.
    [[nodiscard]] const std::map<std::string, const ir::IrModelDeclaration*>&
    getModels() const noexcept;

    // Returns source-owned functions ordered by deterministic source identity.
    [[nodiscard]] const std::map<
        std::string,
        std::vector<const ir::IrFunctionDeclaration*>
    >& getFunctionsBySourceUnit() const noexcept;

    // Returns the source-unit identity owning a declaration.
    [[nodiscard]] std::string sourceUnitIdentity(
        const ir::IrDeclaration& declaration
    ) const;

private:
    // Indexes a single linked program while removing repeated imported declarations.
    void indexProgram(const ir::Program& program);

    // Registers one canonical model and rejects conflicting semantic ownership.
    void registerModel(
        const ir::IrModelDeclaration& model,
        const std::string& sourceUnit
    );

    // Registers one function owned by its original Crossa source unit.
    void registerFunction(
        const ir::IrFunctionDeclaration& function,
        const std::string& sourceUnit
    );

    // Returns one deterministic declaration key without pointer identity.
    [[nodiscard]] static std::string declarationKey(
        const ir::IrDeclaration& declaration,
        const std::string& name
    );

    // Returns a source identity from provenance or falls back to Program identity.
    [[nodiscard]] static std::string sourceIdentity(
        const ir::IrDeclaration& declaration,
        const std::string& fallbackIdentity
    );

    // Returns a path-stable source-unit key for collision-safe file planning.
    [[nodiscard]] static std::string sourceUnitKey(
        const ir::IrDeclaration& declaration,
        const std::string& fallbackIdentity
    );

    std::map<std::string, const ir::IrModelDeclaration*> models_;
    std::map<std::string, std::vector<const ir::IrFunctionDeclaration*>>
        functionsBySourceUnit_;
    std::map<std::string, std::string> modelKeys_;
    std::map<std::string, std::string> functionKeys_;
    std::map<std::string, std::string> functionSymbolKeys_;
};

}
