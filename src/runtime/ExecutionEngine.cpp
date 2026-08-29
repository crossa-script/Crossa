#include "crossa/runtime/ExecutionEngine.h"

#include "crossa/runtime/IrInterpreter.h"

using namespace std;

namespace crossa::runtime {

    // Executes one IR program and reports its runtime progress.
    void ExecutionEngine::execute(
        const compiler::ir::Program& program,
        const utils::Log& log
    ) {
        log.debug("Execution engine started");
        log.debug(
            "Executing IR source unit: " + program.getIdentity() +
            " (" + to_string(program.getDeclarations().size()) +
            " declarations)"
        );
        log.info(
            "Executing Crossa source: " +
                program.getSourcePath().string()
        );
        IrInterpreter interpreter(program, log);
        interpreter.execute();
        log.debug("Execution engine completed");
    }

}
