#pragma once

#include <cstdint>
#include <string>

#include "crossa/compiler/ir/Program.h"

namespace crossa::compiler::generators::native {

// Produces deterministic native operation metadata from one linked IR program.
class NativeProgramGenerator final {
public:
    // Generates the immutable operation identifier declarations for one program.
    [[nodiscard]] std::string generateOperationHeader(
        const ir::Program& program
    ) const;

    // Generates a native factory that reconstructs the compiler validated IR.
    [[nodiscard]] std::string generateProgramHeader() const;

    // Generates the executable Program reconstruction source for one IR program.
    [[nodiscard]] std::string generateProgramSource(
        const ir::Program& program
    ) const;

private:
    // Computes the canonical stable operation identifier used by native bindings.
    [[nodiscard]] static std::uint64_t operationId(
        const std::string& sourceIdentity,
        const ir::IrFunctionDeclaration& function
    ) noexcept;
};

}
