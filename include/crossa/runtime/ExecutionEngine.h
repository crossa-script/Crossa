#pragma once

#include "crossa/compiler/ir/Program.h"
#include "crossa/utils/Log.h"

namespace crossa::runtime {

// Runs the current Crossa IR program through the native execution boundary.
// execute() is the entry point for future runtime instructions.
class ExecutionEngine final {
public:
    // Executes one IR program and reports its runtime progress.
    static void execute(
        const compiler::ir::Program& program,
        const utils::Log& log
    );
};

}
