#include "crossa/compiler/semantic/TypedSourceUnit.h"

#include <utility>

using namespace std;

namespace crossa::compiler::semantic {

    // Creates a typed source unit from its path and declarations.
    TypedSourceUnit::TypedSourceUnit(
        filesystem::path sourcePath,
        string identity,
        vector<unique_ptr<TypedDeclaration>> declarations
    )
        : sourcePath_(std::move(sourcePath)),
          identity_(std::move(identity)),
          declarations_(std::move(declarations)) {}

    // Returns the originating .cra source path.
    const filesystem::path& TypedSourceUnit::getSourcePath() const noexcept {
        return sourcePath_;
    }

    // Returns the deterministic identity derived from the source filename.
    const string& TypedSourceUnit::getIdentity() const noexcept {
        return identity_;
    }

    // Returns the ordered validated declarations.
    const vector<unique_ptr<TypedDeclaration>>&
    TypedSourceUnit::getDeclarations() const noexcept {
        return declarations_;
    }

}
