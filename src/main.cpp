#include <exception>
#include <string>
#include <utility>
#include <vector>

#include "crossa/compiler/ir/Program.h"
#include "crossa/compiler/lexer/Lexer.h"
#include "crossa/compiler/lexer/TokenType.h"
#include "crossa/compiler/source/SourceLoader.h"
#include "crossa/runtime/ExecutionEngine.h"
#include "crossa/utils/Log.h"

using namespace std;
using namespace crossa::compiler::lexer;
using namespace crossa::utils;

// Starts Crossa, loads one source file, and sends it to the execution engine.
int main(int argc, char* argv[]) {
    bool debugEnabled = false;
    int sourceArgumentIndex = 1;

    if (argc == 3 && string(argv[1]) == "--debug") {
        debugEnabled = true;
        sourceArgumentIndex = 2;
    } else if (argc == 3 && string(argv[2]) == "--debug") {
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

        log.debug("Lexer started");
        Lexer lexer(sourceFile);
        const vector<Token> tokens = lexer.tokenize();
        log.debug("Lexer completed: " + to_string(tokens.size()) + " tokens");

        for (const Token& token : tokens) {
            log.debug(
                "Token " + string(TokenTypeUtils::toString(token.getType())) +
                " '" + string(token.getLexeme(sourceFile)) + "' at " +
                to_string(token.getLine()) + ":" +
                to_string(token.getColumn())
            );
        }

        crossa::compiler::ir::Program program(std::move(sourceFile));
        log.debug("Initial IR program created");
        crossa::runtime::ExecutionEngine::execute(program, log);
    } catch (const exception& error) {
        log.error(error.what());
        return 1;
    }

    return 0;
}
