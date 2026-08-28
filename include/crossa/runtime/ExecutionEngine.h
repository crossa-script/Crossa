#pragma once

#include "crossa/compiler/ir/Program.h"
#include "crossa/utils/Log.h"

namespace crossa::runtime {

// Runs a lowered Crossa IR program through the native execution boundary.
// execute() reports the current placeholder runtime stage.
class ExecutionEngine final {
public:
    // Executes one IR program and reports its runtime progress.
    static void execute(
        const compiler::ir::Program& program,
        const utils::Log& log
    );
};

}
