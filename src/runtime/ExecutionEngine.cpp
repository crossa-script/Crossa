#include "crossa/runtime/ExecutionEngine.h"

using namespace std;

namespace crossa::runtime {

    // Executes one IR program and reports its runtime progress.
    void ExecutionEngine::execute(
        const compiler::ir::Program& program,
        const utils::Log& log
    ) {
        log.debug("Execution engine started");
        log.debug("Executing source unit");
        log.info(
            "Executing Crossa source: " +
                program.getSourceFile().getPath().string()
        );
        log.debug("Execution engine completed");
    }

}
