#include "crossa/runtime/NativeOperationCatalog.h"

#include <stdexcept>
#include <filesystem>

using namespace std;

namespace crossa::runtime {

    // Indexes every function declared in one linked Crossa IR program.
    NativeOperationCatalog::NativeOperationCatalog(
        const compiler::ir::Program& program
    ) {
        for (const unique_ptr<compiler::ir::IrDeclaration>& declaration :
             program.getDeclarations()) {
            if (declaration->getKind() !=
                compiler::ir::IrDeclarationKind::Function) {
                continue;
            }
            const auto& function = static_cast<
                const compiler::ir::IrFunctionDeclaration&>(*declaration);
            const uint64_t id = operationId(sourceIdentity(program, function), function);
            if (!functions_.emplace(id, &function).second) {
                throw runtime_error("Crossa generated operation ID collision.");
            }
        }
    }

    // Returns the function selected by a generated operation identifier.
    const compiler::ir::IrFunctionDeclaration* NativeOperationCatalog::find(
        uint64_t operationId
    ) const noexcept {
        const auto found = functions_.find(operationId);
        return found == functions_.end() ? nullptr : found->second;
    }

    // Derives the generated operation identifier from one source identity and function.
    uint64_t NativeOperationCatalog::operationId(
        const string& sourceIdentity,
        const compiler::ir::IrFunctionDeclaration& function
    ) noexcept {
        uint64_t value = 1469598103934665603ULL;
        const string signature = sourceIdentity + ":" + function.getName() +
            ":" + function.getReturnType().format();
        for (const char character : signature) {
            value ^= static_cast<unsigned char>(character);
            value *= 1099511628211ULL;
        }
        return value;
    }

    // Returns the source-unit identity stored on one reconstructed function.
    string NativeOperationCatalog::sourceIdentity(
        const compiler::ir::Program& program,
        const compiler::ir::IrFunctionDeclaration& function
    ) {
        const string_view sourcePath = function.getLocation().getSourcePath();
        return sourcePath.empty()
            ? program.getIdentity()
            : filesystem::path(string(sourcePath)).stem().string();
    }

}
