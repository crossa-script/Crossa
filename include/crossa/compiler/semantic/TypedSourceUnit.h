#pragma once

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#include "crossa/compiler/semantic/TypedDeclaration.h"

namespace crossa::compiler::semantic {

// Owns the validated typed declarations produced for one .cra file.
// The source path preserves deterministic source-unit identity.
class TypedSourceUnit final {
public:
    // Creates a typed source unit from its path and declarations.
    TypedSourceUnit(
        std::filesystem::path sourcePath,
        std::string identity,
        std::vector<std::unique_ptr<TypedDeclaration>> declarations
    );

    // Returns the originating .cra source path.
    [[nodiscard]] const std::filesystem::path& getSourcePath() const noexcept;

    // Returns the deterministic identity derived from the source filename.
    [[nodiscard]] const std::string& getIdentity() const noexcept;

    // Returns the ordered validated declarations.
    [[nodiscard]] const std::vector<std::unique_ptr<TypedDeclaration>>&
    getDeclarations() const noexcept;

private:
    std::filesystem::path sourcePath_;
    std::string identity_;
    std::vector<std::unique_ptr<TypedDeclaration>> declarations_;
};

}
