#include "crossa/runtime/ExecutionEngine.h"

#include "crossa/network/NetworkEngine.h"
#include "crossa/runtime/IrInterpreter.h"
#include "crossa/runtime/RuntimeConfiguration.h"
#include "crossa/runtime/scheduler/TaskScheduler.h"

using namespace std;

namespace crossa::runtime {

    // Executes one IR program and reports its runtime progress.
    void ExecutionEngine::execute(
        const compiler::ir::Program& program,
        const compiler::ir::Program* configurationProgram,
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
        RuntimeConfiguration configuration = RuntimeConfiguration::load(
            program,
            configurationProgram
        );
        log.debug(
            "Runtime configuration loaded: baseUrl=" +
            string(
                configuration.getNetworkConfiguration().getBaseUrl().empty()
                    ? "not-set"
                    : "set"
            ) +
            " workers=" + to_string(
                configuration.getSchedulerOptions().getWorkerCount()
            ) +
            " queueCapacity=" + to_string(
                configuration.getSchedulerOptions().getMaximumQueuedTasks()
            )
        );
        scheduler::TaskScheduler scheduler(
            configuration.getSchedulerOptions(),
            log
        );
        network::NetworkEngine networkEngine(
            configuration.getNetworkConfiguration(),
            log,
            configuration.getSchedulerOptions().getWorkerCount()
        );
        IrInterpreter interpreter(
            program,
            log,
            scheduler,
            networkEngine,
            configuration.getNetworkConfiguration()
        );
        interpreter.execute();
        scheduler.waitUntilIdle();
        log.debug("Execution engine completed");
    }

}
