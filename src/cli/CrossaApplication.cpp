#include "crossa/cli/CrossaApplication.h"

#include <exception>
#include <string>
#include <string_view>
#include <utility>

#include "crossa/compiler/ast/AstPrinter.h"
#include "crossa/compiler/ir/Program.h"
#include "crossa/compiler/lexer/Lexer.h"
#include "crossa/compiler/lexer/TokenType.h"
#include "crossa/compiler/parser/Parser.h"
#include "crossa/compiler/source/SourceLoader.h"
#include "crossa/runtime/ExecutionEngine.h"

using namespace std;

namespace crossa::cli {

    // Runs the Crossa command-line application and returns its process status.
    int CrossaApplication::run(int argc, char* argv[]) {
        const optional<Arguments> arguments = parseArguments(argc, argv);
        if (!arguments.has_value()) {
            utils::Log().error("Usage: crossa [--debug] <file.cra>");
            return 1;
        }

        const utils::Log log(
            arguments->debugEnabled
                ? utils::Log::Level::Debug
                : utils::Log::Level::Error
        );
        log.debug("Crossa started");
        log.debug("Source argument received");

        try {
            executeSource(*arguments, log);
        } catch (const exception& error) {
            log.error(error.what());
            return 1;
        }

        return 0;
    }

    // Parses supported command-line arguments into execution options.
    optional<CrossaApplication::Arguments> CrossaApplication::parseArguments(
        int argc,
        char* argv[]
    ) {
        if (argc == 2 && string_view(argv[1]) != "--debug") {
            return Arguments{false, argv[1]};
        }

        if (argc == 3 && string_view(argv[1]) == "--debug") {
            return Arguments{true, argv[2]};
        }

        if (argc == 3 && string_view(argv[2]) == "--debug") {
            return Arguments{true, argv[1]};
        }

        return nullopt;
    }

    // Loads, tokenizes, and executes one Crossa source file.
    void CrossaApplication::executeSource(
        const Arguments& arguments,
        const utils::Log& log
    ) {
        compiler::source::SourceFile sourceFile =
            compiler::source::SourceLoader::load(arguments.sourcePath, log);
        const vector<compiler::lexer::Token> tokens =
            tokenizeSource(sourceFile, log);
        logTokens(tokens, sourceFile, log);
        compiler::ast::SourceUnit sourceUnit =
            parseSource(tokens, sourceFile, log);
        logAst(sourceUnit, log);

        compiler::ir::Program program(std::move(sourceFile));
        log.debug("Initial IR program created");
        runtime::ExecutionEngine::execute(program, log);
    }

    // Tokenizes one source file and reports the lexer lifecycle.
    vector<compiler::lexer::Token> CrossaApplication::tokenizeSource(
        const compiler::source::SourceFile& sourceFile,
        const utils::Log& log
    ) {
        log.debug("Lexer started");
        compiler::lexer::Lexer lexer(sourceFile);
        vector<compiler::lexer::Token> tokens = lexer.tokenize();
        log.debug("Lexer completed: " + to_string(tokens.size()) + " tokens");
        return tokens;
    }

    // Parses one token stream and reports the parser lifecycle.
    compiler::ast::SourceUnit CrossaApplication::parseSource(
        const vector<compiler::lexer::Token>& tokens,
        const compiler::source::SourceFile& sourceFile,
        const utils::Log& log
    ) {
        log.debug("Parser started");
        compiler::parser::Parser parser(tokens, sourceFile);
        compiler::ast::SourceUnit sourceUnit = parser.parse();
        log.debug(
            "Parser completed: " +
            to_string(sourceUnit.getDeclarations().size()) +
            " declarations"
        );
        return sourceUnit;
    }

    // Writes every emitted token when debug logging is enabled.
    void CrossaApplication::logTokens(
        const vector<compiler::lexer::Token>& tokens,
        const compiler::source::SourceFile& sourceFile,
        const utils::Log& log
    ) {
        for (const compiler::lexer::Token& token : tokens) {
            const string typeName(
                compiler::lexer::TokenTypeUtils::toString(token.getType())
            );
            const string lexeme(token.getLexeme(sourceFile));
            const string location =
                to_string(token.getLine()) + ":" +
                to_string(token.getColumn());

            log.debug(
                "Token " + typeName + " '" + lexeme + "' at " + location
            );
        }
    }

    // Writes a concise summary for every parsed AST declaration.
    void CrossaApplication::logAst(
        const compiler::ast::SourceUnit& sourceUnit,
        const utils::Log& log
    ) {
        const vector<string> summaries =
            compiler::ast::AstPrinter::summarize(sourceUnit);
        for (const string& summary : summaries) {
            log.debug(summary);
        }
    }

}
