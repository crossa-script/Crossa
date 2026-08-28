#include "crossa/cli/CrossaApplication.h"

#include <exception>
#include <string>
#include <string_view>
#include <utility>

#include "crossa/compiler/ast/AstPrinter.h"
#include "crossa/compiler/ir/IrLowerer.h"
#include "crossa/compiler/ir/IrPrinter.h"
#include "crossa/compiler/ir/Program.h"
#include "crossa/compiler/lexer/Lexer.h"
#include "crossa/compiler/lexer/TokenType.h"
#include "crossa/compiler/parser/Parser.h"
#include "crossa/compiler/semantic/SemanticAnalyzer.h"
#include "crossa/compiler/semantic/SemanticModelPrinter.h"
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
        logStepStarted(1, "Source loading", log);
        compiler::source::SourceFile sourceFile =
            compiler::source::SourceLoader::load(arguments.sourcePath, log);
        logStepCompleted(1, "Source loading", log);

        logStepStarted(2, "Lexical analysis", log);
        const vector<compiler::lexer::Token> tokens =
            tokenizeSource(sourceFile, log);
        logTokens(tokens, sourceFile, log);
        logStepCompleted(2, "Lexical analysis", log);

        logStepStarted(3, "Parsing and AST creation", log);
        compiler::ast::SourceUnit sourceUnit =
            parseSource(tokens, sourceFile, log);
        logAst(sourceUnit, log);
        logStepCompleted(3, "Parsing and AST creation", log);

        logStepStarted(4, "Semantic analysis and typed model", log);
        compiler::semantic::TypedSourceUnit semanticModel =
            analyzeSource(sourceUnit, sourceFile, log);
        logSemanticModel(semanticModel, log);
        logStepCompleted(4, "Semantic analysis and typed model", log);

        logStepStarted(5, "Typed IR lowering", log);
        compiler::ir::Program program =
            compiler::ir::IrLowerer::lower(semanticModel);
        logIr(program, log);
        log.debug(
            "Typed IR lowering completed: " +
            to_string(program.getDeclarations().size()) +
            " IR declarations"
        );
        logStepCompleted(5, "Typed IR lowering", log);

        logStepStarted(6, "Native execution", log);
        runtime::ExecutionEngine::execute(program, log);
        logStepCompleted(6, "Native execution", log);
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

    // Validates one AST and produces its typed semantic model.
    compiler::semantic::TypedSourceUnit CrossaApplication::analyzeSource(
        const compiler::ast::SourceUnit& sourceUnit,
        const compiler::source::SourceFile& sourceFile,
        const utils::Log& log
    ) {
        log.debug("Semantic analysis started");
        compiler::semantic::SemanticAnalyzer analyzer(sourceUnit, sourceFile);
        compiler::semantic::TypedSourceUnit semanticModel = analyzer.analyze();
        log.debug(
            "Semantic source identity resolved: " +
            semanticModel.getIdentity()
        );
        log.debug(
            "Semantic analysis completed: " +
            to_string(semanticModel.getDeclarations().size()) +
            " typed declarations"
        );
        return semanticModel;
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

    // Writes a concise summary for every typed semantic declaration.
    void CrossaApplication::logSemanticModel(
        const compiler::semantic::TypedSourceUnit& sourceUnit,
        const utils::Log& log
    ) {
        const vector<string> summaries =
            compiler::semantic::SemanticModelPrinter::summarize(sourceUnit);
        for (const string& summary : summaries) {
            log.debug(summary);
        }
    }

    // Writes a readable summary for every lowered IR instruction.
    void CrossaApplication::logIr(
        const compiler::ir::Program& program,
        const utils::Log& log
    ) {
        const vector<string> summaries =
            compiler::ir::IrPrinter::summarize(program);
        for (const string& summary : summaries) {
            log.debug(summary);
        }
    }

    // Reports that one numbered Crossa pipeline step has started.
    void CrossaApplication::logStepStarted(
        size_t step,
        const string& name,
        const utils::Log& log
    ) {
        log.debug(
            "Step " + to_string(step) + "/" + to_string(TotalSteps) +
            " started: " + name
        );
    }

    // Reports that one numbered Crossa pipeline step has completed.
    void CrossaApplication::logStepCompleted(
        size_t step,
        const string& name,
        const utils::Log& log
    ) {
        log.debug(
            "Step " + to_string(step) + "/" + to_string(TotalSteps) +
            " completed: " + name
        );
    }

}
