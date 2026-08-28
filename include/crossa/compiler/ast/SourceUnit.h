#pragma once

#include <memory>
#include <vector>

#include "crossa/compiler/ast/Declaration.h"

namespace crossa::compiler::ast {

// Owns the ordered AST declarations parsed from one .cra source file.
// getDeclarations() provides the root input for semantic analysis.
class SourceUnit final {
public:
    // Creates a source unit from its ordered top-level declarations.
    explicit SourceUnit(std::vector<std::unique_ptr<Declaration>> declarations);

    // Returns the ordered top-level declarations.
    [[nodiscard]] const std::vector<std::unique_ptr<Declaration>>&
    getDeclarations() const noexcept;

private:
    std::vector<std::unique_ptr<Declaration>> declarations_;
};

}
