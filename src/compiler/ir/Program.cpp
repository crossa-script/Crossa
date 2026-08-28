#include "crossa/compiler/ir/Program.h"

#include <utility>

using namespace std;

namespace crossa::compiler::ir {

    // Creates an IR program from a source identity and lowered declarations.
    Program::Program(
        filesystem::path sourcePath,
        string identity,
        vector<unique_ptr<IrDeclaration>> declarations
    )
        : sourcePath_(std::move(sourcePath)),
          identity_(std::move(identity)),
          declarations_(std::move(declarations)) {}

    // Returns the source path represented by this IR program.
    const filesystem::path& Program::getSourcePath() const noexcept {
        return sourcePath_;
    }

    // Returns the deterministic source-unit identity.
    const string& Program::getIdentity() const noexcept {
        return identity_;
    }

    // Returns the ordered lowered IR declarations.
    const vector<unique_ptr<IrDeclaration>>&
    Program::getDeclarations() const noexcept {
        return declarations_;
    }

}
