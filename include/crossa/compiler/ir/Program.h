#pragma once

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#include "crossa/compiler/ir/IrDeclaration.h"

namespace crossa::compiler::ir {

// Owns the platform-neutral IR for one validated Crossa source unit.
// Declarations are ordered deterministically for execution and generation.
class Program final {
public:
    // Creates an IR program from a source identity and lowered declarations.
    Program(
        std::filesystem::path sourcePath,
        std::string identity,
        std::vector<std::unique_ptr<IrDeclaration>> declarations
    );

    // Returns the source path represented by this IR program.
    [[nodiscard]] const std::filesystem::path& getSourcePath() const noexcept;

    // Returns the deterministic source-unit identity.
    [[nodiscard]] const std::string& getIdentity() const noexcept;

    // Returns the ordered lowered IR declarations.
    [[nodiscard]] const std::vector<std::unique_ptr<IrDeclaration>>&
    getDeclarations() const noexcept;

private:
    std::filesystem::path sourcePath_;
    std::string identity_;
    std::vector<std::unique_ptr<IrDeclaration>> declarations_;
};

}
