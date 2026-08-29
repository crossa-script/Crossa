#pragma once

#include "crossa/compiler/ir/Program.h"
#include "crossa/utils/Log.h"

namespace crossa::runtime {

// Runs a lowered Crossa IR program through the native execution boundary.
// execute() initializes globals and executes top-level calls in source order.
class ExecutionEngine final {
public:
    // Executes one IR program and reports its runtime progress.
    static void execute(
        const compiler::ir::Program& program,
        const compiler::ir::Program* configurationProgram,
        const utils::Log& log
    );
};

}
