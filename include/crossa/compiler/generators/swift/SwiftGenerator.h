#pragma once

#include <string>
#include <vector>

#include "crossa/compiler/generators/swift/SwiftGeneratedSource.h"
#include "crossa/compiler/ir/Program.h"

namespace crossa::compiler::generators::swift {

// Emits deterministic project-wide Swift APIs from linked Crossa IR.
// generateProject() owns model de-duplication, pure translation, and native wrappers.
class SwiftGenerator final {
public:
    // Generates separated Swift model and API files for one linked Crossa project.
    [[nodiscard]] std::vector<SwiftGeneratedSource> generateProject(
        const std::vector<const ir::Program*>& programs
    ) const;
};

}
