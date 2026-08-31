#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>

#include "crossa/compiler/ir/Program.h"

namespace crossa::runtime {

// Indexes immutable IR functions by deterministic generated Android operation IDs.
class NativeOperationCatalog final {
public:
    // Indexes every function declared in one linked Crossa IR program.
    explicit NativeOperationCatalog(const compiler::ir::Program& program);

    // Returns the function selected by a generated operation identifier.
    [[nodiscard]] const compiler::ir::IrFunctionDeclaration* find(
        std::uint64_t operationId
    ) const noexcept;

    // Derives the generated operation identifier from one source identity and function.
    [[nodiscard]] static std::uint64_t operationId(
        const std::string& sourceIdentity,
        const compiler::ir::IrFunctionDeclaration& function
    ) noexcept;

private:
    // Returns the source-unit identity stored on one reconstructed function.
    [[nodiscard]] static std::string sourceIdentity(
        const compiler::ir::Program& program,
        const compiler::ir::IrFunctionDeclaration& function
    );

    std::unordered_map<std::uint64_t, const compiler::ir::IrFunctionDeclaration*>
        functions_;
};

}
