#include <exception>
#include <string>
#include <utility>

#include "crossa/compiler/ir/Program.h"
#include "crossa/compiler/source/SourceLoader.h"
#include "crossa/runtime/ExecutionEngine.h"
#include "crossa/utils/Log.h"

using namespace crossa::utils;

// Starts Crossa, loads one source file, and sends it to the execution engine.
int main(int argc, char* argv[]) {
    bool debugEnabled = false;
    int sourceArgumentIndex = 1;

    if (argc == 3 && std::string(argv[1]) == "--debug") {
        debugEnabled = true;
        sourceArgumentIndex = 2;
    } else if (argc == 3 && std::string(argv[2]) == "--debug") {
        debugEnabled = true;
    } else if (argc != 2) {
        Log().error("Usage: crossa [--debug] <file.cra>");
        return 1;
    }

    Log log(debugEnabled ? Log::Level::Debug : Log::Level::Error);
    log.debug("Crossa started");
    log.debug("Source argument received");

    try {
        auto sourceFile = crossa::compiler::source::SourceLoader::load(
            argv[sourceArgumentIndex], log
        );
        crossa::compiler::ir::Program program(std::move(sourceFile));
        log.debug("Initial IR program created");
        crossa::runtime::ExecutionEngine::execute(program, log);
    } catch (const std::exception& error) {
        log.error(error.what());
        return 1;
    }

    return 0;
}
