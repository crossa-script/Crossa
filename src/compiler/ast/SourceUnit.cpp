#include "crossa/compiler/ast/SourceUnit.h"

#include <utility>

using namespace std;

namespace crossa::compiler::ast {

    // Creates a source unit from its ordered top-level declarations.
    SourceUnit::SourceUnit(vector<unique_ptr<Declaration>> declarations)
        : declarations_(std::move(declarations)) {}

    // Returns the ordered top-level declarations.
    const vector<unique_ptr<Declaration>>&
    SourceUnit::getDeclarations() const noexcept {
        return declarations_;
    }

    // Transfers declaration ownership to the project linker.
    vector<unique_ptr<Declaration>> SourceUnit::takeDeclarations() noexcept {
        return std::move(declarations_);
    }

}
