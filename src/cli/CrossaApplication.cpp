#include "crossa/cli/CrossaApplication.h"

#include <exception>
#include <stdexcept>
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
#include "crossa/compiler/project/ProjectLinker.h"
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
            utils::Log().error(
                "Usage: crossa [--debug] <file.cra>"
            );
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
        if (argc < 2) {
            return nullopt;
        }

        bool debugEnabled = false;
        optional<filesystem::path> sourcePath;

        for (int index = 1; index < argc; ++index) {
            const string_view argument(argv[index]);
            if (argument == "--debug") {
                debugEnabled = true;
                continue;
            }

            if (!argument.empty() && argument.front() == '-') {
                return nullopt;
            }
            if (sourcePath.has_value()) {
                return nullopt;
            }
            sourcePath = filesystem::path(argument);
        }

        if (!sourcePath.has_value()) {
            return nullopt;
        }
        return Arguments{debugEnabled, std::move(sourcePath.value())};
    }

    // Loads, tokenizes, and executes one Crossa source file.
    void CrossaApplication::executeSource(
        const Arguments& arguments,
        const utils::Log& log
    ) {
        logStepStarted(1, "Source loading", log);
        const filesystem::path entryPath =
            filesystem::weakly_canonical(arguments.sourcePath);
        compiler::source::SourceFile sourceFile =
            compiler::source::SourceLoader::load(entryPath, log);
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

        logStepStarted(4, "Import graph resolution and project linking", log);
        sourceUnit = linkProject(
            sourceFile.getPath(),
            std::move(sourceUnit),
            log
        );
        logAst(sourceUnit, log);
        logStepCompleted(
            4,
            "Import graph resolution and project linking",
            log
        );

        logStepStarted(5, "Semantic analysis and typed model", log);
        compiler::semantic::TypedSourceUnit semanticModel =
            analyzeSource(sourceUnit, sourceFile, log);
        logSemanticModel(semanticModel, log);
        logStepCompleted(5, "Semantic analysis and typed model", log);

        logStepStarted(6, "Typed IR lowering", log);
        compiler::ir::Program program =
            compiler::ir::IrLowerer::lower(semanticModel);
        logIr(program, log);
        log.debug(
            "Typed IR lowering completed: " +
            to_string(program.getDeclarations().size()) +
            " IR declarations"
        );
        logStepCompleted(6, "Typed IR lowering", log);

        logStepStarted(7, "Native execution", log);
        optional<compiler::ir::Program> configurationProgram =
            compileSiblingConfiguration(sourceFile.getPath(), log);
        runtime::ExecutionEngine::execute(
            program,
            configurationProgram.has_value()
                ? &configurationProgram.value()
                : nullptr,
            log
        );
        logStepCompleted(7, "Native execution", log);
    }

    // Compiles an optional sibling config.cra into declarative IR.
    optional<compiler::ir::Program>
    CrossaApplication::compileSiblingConfiguration(
        const filesystem::path& sourcePath,
        const utils::Log& log
    ) {
        if (sourcePath.filename() == "config.cra") {
            return nullopt;
        }
        const filesystem::path configurationPath =
            sourcePath.parent_path() / "config.cra";
        if (!filesystem::exists(configurationPath)) {
            log.debug("No sibling config.cra found; runtime defaults selected");
            return nullopt;
        }

        log.debug("Compiling sibling runtime configuration: " +
                  configurationPath.string());
        compiler::source::SourceFile sourceFile =
            compiler::source::SourceLoader::load(configurationPath, log);
        const vector<compiler::lexer::Token> tokens =
            tokenizeSource(sourceFile, log);
        compiler::ast::SourceUnit sourceUnit =
            parseSource(tokens, sourceFile, log);
        compiler::semantic::TypedSourceUnit semanticModel =
            analyzeSource(sourceUnit, sourceFile, log);
        compiler::ir::Program program =
            compiler::ir::IrLowerer::lower(semanticModel);
        validateConfigurationProgram(program);
        log.debug("Sibling runtime configuration compiled");
        return program;
    }

    // Ensures a sibling configuration file contains only config declarations.
    void CrossaApplication::validateConfigurationProgram(
        const compiler::ir::Program& program
    ) {
        size_t configCount = 0;
        for (const unique_ptr<compiler::ir::IrDeclaration>& declaration :
             program.getDeclarations()) {
            if (declaration->getKind() !=
                compiler::ir::IrDeclarationKind::Config) {
                throw runtime_error(
                    "config.cra may contain only one declarative config block."
                );
            }
            ++configCount;
        }
        if (configCount != 1) {
            throw runtime_error(
                "config.cra must contain exactly one config block."
            );
        }
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

    // Resolves the entry file's transitive imports into one project AST.
    compiler::ast::SourceUnit CrossaApplication::linkProject(
        const filesystem::path& entryPath,
        compiler::ast::SourceUnit entrySourceUnit,
        const utils::Log& log
    ) {
        log.debug("Project import graph resolution started");
        return compiler::project::ProjectLinker::link(
            filesystem::current_path(),
            entryPath,
            std::move(entrySourceUnit),
            log
        );
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
