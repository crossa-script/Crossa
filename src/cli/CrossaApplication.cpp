#include "crossa/cli/CrossaApplication.h"

#include <algorithm>
#include <exception>
#include <fstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>

#include "crossa/cli/CrossaVersion.h"
#include "crossa/cli/doctor/DoctorCommand.h"
#include "crossa/compiler/ast/AstPrinter.h"
#include "crossa/compiler/generators/kotlin/KotlinGenerator.h"
#include "crossa/compiler/ir/IrDeclaration.h"
#include "crossa/compiler/ir/IrExpression.h"
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
#include "crossa/packaging/android/AndroidProjectGenerator.h"
#include "crossa/runtime/ExecutionEngine.h"
#include "crossa/utils/PrintUtils.h"

using namespace std;

namespace crossa::cli {

    // Runs the Crossa command-line application and returns its process status.
    int CrossaApplication::run(int argc, char* argv[]) {
        if (argc == 2 && string_view(argv[1]) == "--help") {
            printUsage();
            return 0;
        }
        if (argc == 2 && string_view(argv[1]) == "-h") {
            printUsage();
            return 0;
        }
        if (argc == 2 && string_view(argv[1]) == "--version") {
            utils::PrintUtils::println(CrossaVersion::current());
            return 0;
        }
        if (argc == 2 && string_view(argv[1]) == "doctor") {
            return doctor::DoctorCommand::run(argv[0]);
        }

        const optional<Arguments> arguments = parseArguments(argc, argv);
        if (!arguments.has_value()) {
            utils::Log().error("Invalid Crossa command.");
            printUsage();
            return 1;
        }

        const utils::Log log(
            arguments->debugEnabled
                ? utils::Log::Level::Debug
                : utils::Log::Level::Error
        );
        log.debug("Crossa started");
        log.debug("Source argument received");
        log.debug(
            arguments->command == Command::Check
                ? "Command selected: check"
                : arguments->command == Command::Test
                    ? "Command selected: test"
                    : arguments->command == Command::GenerateKotlin
                        ? "Command selected: generate kotlin"
                        : arguments->command == Command::GenerateAndroidLibrary
                            ? "Command selected: generate-build android"
                        : "Command selected: run"
        );

        try {
            executeSource(*arguments, log);
        } catch (const exception& error) {
            log.error(error.what());
            return 1;
        }

        return 0;
    }

    // Prints the supported Crossa command-line usage.
    void CrossaApplication::printUsage() {
        utils::PrintUtils::println(
            "Usage: crossa [check|run|test] [--debug] <file.cra>\n"
            "       crossa generate kotlin [--debug] <file.cra> "
            "--output <directory>\n"
            "       crossa generate-build android [--debug] "
            "<project-directory> --output <directory>\n"
            "       crossa --version\n"
            "       crossa doctor"
        );
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
        Command command = Command::Run;
        bool commandProvided = false;
        optional<filesystem::path> sourcePath;
        optional<filesystem::path> outputDirectory;

        for (int index = 1; index < argc; ++index) {
            const string_view argument(argv[index]);
            if (argument == "--debug") {
                debugEnabled = true;
                continue;
            }

            if (argument == "--output") {
                if (outputDirectory.has_value() || index + 1 >= argc) {
                    return nullopt;
                }
                const string_view outputArgument(argv[++index]);
                if (outputArgument.empty() || outputArgument.front() == '-') {
                    return nullopt;
                }
                outputDirectory = filesystem::path(outputArgument);
                continue;
            }

            if (argument == "generate") {
                if (commandProvided || sourcePath.has_value() ||
                    index + 1 >= argc ||
                    string_view(argv[++index]) != "kotlin") {
                    return nullopt;
                }
                command = Command::GenerateKotlin;
                commandProvided = true;
                continue;
            }

            if (argument == "generate-build") {
                if (commandProvided || sourcePath.has_value() ||
                    index + 1 >= argc ||
                    string_view(argv[++index]) != "android") {
                    return nullopt;
                }
                command = Command::GenerateAndroidLibrary;
                commandProvided = true;
                continue;
            }

            const optional<Command> parsedCommand = parseCommand(argument);
            if (parsedCommand.has_value()) {
                if (commandProvided || sourcePath.has_value()) {
                    return nullopt;
                }
                command = parsedCommand.value();
                commandProvided = true;
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
        const bool generatesOutput = command == Command::GenerateKotlin ||
            command == Command::GenerateAndroidLibrary;
        if (generatesOutput != outputDirectory.has_value()) {
            return nullopt;
        }
        return Arguments{
            command,
            debugEnabled,
            std::move(sourcePath.value()),
            std::move(outputDirectory)
        };
    }

    // Parses one optional CLI command name.
    optional<CrossaApplication::Command>
    CrossaApplication::parseCommand(string_view argument) noexcept {
        if (argument == "check") {
            return Command::Check;
        }
        if (argument == "run") {
            return Command::Run;
        }
        if (argument == "test") {
            return Command::Test;
        }
        return nullopt;
    }

    // Loads, compiles, and applies the requested workflow to one Crossa source file.
    void CrossaApplication::executeSource(
        const Arguments& arguments,
        const utils::Log& log
    ) {
        if (arguments.command == Command::GenerateAndroidLibrary) {
            executeAndroidLibraryBuild(arguments, log);
            return;
        }

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

        string finalStepName;
        switch (arguments.command) {
            case Command::Check:
                finalStepName = "Check completion (execution skipped)";
                break;
            case Command::Test:
                finalStepName = "Test execution";
                break;
            case Command::GenerateKotlin:
                finalStepName = "Kotlin source generation";
                break;
            case Command::GenerateAndroidLibrary:
                finalStepName = "Android library generation";
                break;
            case Command::Run:
                finalStepName = "Native execution";
                break;
        }
        logStepStarted(7, finalStepName, log);
        if (arguments.command == Command::Check) {
            log.debug(
                "Check completed successfully; configuration and execution "
                "were skipped"
            );
            logStepCompleted(7, finalStepName, log);
            return;
        }

        if (arguments.command == Command::GenerateKotlin) {
            const optional<compiler::ir::Program> configurationProgram =
                compileSiblingConfiguration(sourceFile.getPath(), log);
            compiler::generators::kotlin::KotlinGenerator generator;
            const compiler::generators::kotlin::KotlinGeneratedSource source =
                generator.generate(
                    program,
                    readKotlinPackageName(configurationProgram)
                );
            writeGeneratedKotlinSource(
                source,
                arguments.outputDirectory.value(),
                log
            );
            logStepCompleted(7, finalStepName, log);
            return;
        }

        optional<compiler::ir::Program> configurationProgram =
            compileSiblingConfiguration(sourceFile.getPath(), log);
        runtime::ExecutionEngine::execute(
            program,
            configurationProgram.has_value()
                ? &configurationProgram.value()
                : nullptr,
            log,
            arguments.command == Command::Test
                ? runtime::ExecutionMode::Test
                : runtime::ExecutionMode::Run
        );
        logStepCompleted(7, finalStepName, log);
    }

    // Compiles every project source and writes the generated Android library project.
    void CrossaApplication::executeAndroidLibraryBuild(
        const Arguments& arguments,
        const utils::Log& log
    ) {
        const filesystem::path projectDirectory =
            filesystem::weakly_canonical(arguments.sourcePath);
        if (!filesystem::is_directory(projectDirectory)) {
            throw runtime_error(
                "generate-build android requires a project directory: " +
                projectDirectory.string()
            );
        }

        logStepStarted(1, "Android project source discovery", log);
        const vector<filesystem::path> sourcePaths =
            discoverProjectSources(projectDirectory);
        if (sourcePaths.empty()) {
            throw runtime_error(
                "No non-configuration .cra files were found in: " +
                projectDirectory.string()
            );
        }
        logStepCompleted(1, "Android project source discovery", log);

        const optional<compiler::ir::Program> configurationProgram =
            compileSiblingConfiguration(projectDirectory / "project.cra", log);
        const optional<string> packageName =
            readKotlinPackageName(configurationProgram);
        const optional<string> kotlinPackageName = packageName.has_value()
            ? packageName
            : optional<string>("io.crossa.generated");
        compiler::generators::kotlin::KotlinGenerator generator;
        vector<compiler::generators::kotlin::KotlinGeneratedSource> sources;
        sources.reserve(sourcePaths.size());
        for (const filesystem::path& sourcePath : sourcePaths) {
            compiler::source::SourceFile sourceFile =
                compiler::source::SourceLoader::load(sourcePath, log);
            const vector<compiler::lexer::Token> tokens =
                tokenizeSource(sourceFile, log);
            compiler::ast::SourceUnit sourceUnit =
                parseSource(tokens, sourceFile, log);
            sourceUnit = linkProject(
                sourceFile.getPath(),
                std::move(sourceUnit),
                log
            );
            const compiler::semantic::TypedSourceUnit semanticModel =
                analyzeSource(sourceUnit, sourceFile, log);
            const compiler::ir::Program program =
                compiler::ir::IrLowerer::lower(semanticModel);
            sources.push_back(generator.generate(
                program,
                kotlinPackageName,
                compiler::generators::kotlin::KotlinGenerationTarget::AndroidNative
            ));
        }

        logStepStarted(7, "Android Gradle library project generation", log);
        packaging::android::AndroidProjectGenerator projectGenerator;
        projectGenerator.generate(
            sources,
            packageName,
            arguments.outputDirectory.value()
        );
        logStepCompleted(7, "Android Gradle library project generation", log);
    }

    // Discovers every non-configuration Crossa source in a project directory.
    vector<filesystem::path> CrossaApplication::discoverProjectSources(
        const filesystem::path& projectDirectory
    ) {
        vector<filesystem::path> sourcePaths;
        error_code error;
        filesystem::recursive_directory_iterator iterator(
            projectDirectory,
            filesystem::directory_options::skip_permission_denied,
            error
        );
        if (error) {
            throw runtime_error(
                "Unable to inspect Android project directory: " +
                projectDirectory.string()
            );
        }
        const filesystem::recursive_directory_iterator end;
        while (iterator != end) {
            if (iterator->is_regular_file(error) &&
                iterator->path().extension() == ".cra" &&
                iterator->path().filename() != "config.cra") {
                sourcePaths.push_back(iterator->path());
            }
            iterator.increment(error);
            if (error) {
                throw runtime_error(
                    "Unable to inspect Android project directory: " +
                    projectDirectory.string()
                );
            }
        }
        sort(sourcePaths.begin(), sourcePaths.end());
        return sourcePaths;
    }

    // Writes one generated Kotlin source unit into the requested output directory.
    void CrossaApplication::writeGeneratedKotlinSource(
        const compiler::generators::kotlin::KotlinGeneratedSource& source,
        const filesystem::path& outputDirectory,
        const utils::Log& log
    ) {
        error_code error;
        filesystem::create_directories(outputDirectory, error);
        if (error || !filesystem::is_directory(outputDirectory, error)) {
            throw runtime_error(
                "Unable to create Kotlin output directory: " +
                outputDirectory.string()
            );
        }

        const filesystem::path outputPath =
            outputDirectory / source.getFileName();
        filesystem::path temporaryPath = outputPath;
        temporaryPath += ".tmp";
        ofstream output(temporaryPath, ios::binary | ios::trunc);
        if (!output.is_open()) {
            throw runtime_error(
                "Unable to write Kotlin output file: " +
                temporaryPath.string()
            );
        }

        output.write(
            source.getContent().data(),
            static_cast<streamsize>(source.getContent().size())
        );
        output.close();
        if (!output) {
            filesystem::remove(temporaryPath, error);
            throw runtime_error(
                "Unable to finish Kotlin output file: " +
                temporaryPath.string()
            );
        }

        filesystem::rename(temporaryPath, outputPath, error);
        if (error) {
            filesystem::remove(temporaryPath, error);
            throw runtime_error(
                "Unable to finalize Kotlin output file: " +
                outputPath.string()
            );
        }
        log.debug("Kotlin source generated: " + outputPath.string());
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
            log.debug("No sibling config.cra found");
            return nullopt;
        }

        log.debug("Compiling sibling configuration: " +
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
        log.debug("Sibling configuration compiled");
        return program;
    }

    // Reads the optional Kotlin package name from declarative configuration IR.
    optional<string> CrossaApplication::readKotlinPackageName(
        const optional<compiler::ir::Program>& configurationProgram
    ) {
        if (!configurationProgram.has_value()) {
            return nullopt;
        }

        for (const unique_ptr<compiler::ir::IrDeclaration>& declaration :
             configurationProgram->getDeclarations()) {
            if (declaration->getKind() !=
                compiler::ir::IrDeclarationKind::Config) {
                continue;
            }
            const auto& configuration = static_cast<
                const compiler::ir::IrConfigDeclaration&
            >(*declaration);
            for (const compiler::ir::IrConfigEntry& entry :
                 configuration.getEntries()) {
                if (entry.getName() != "packageName") {
                    continue;
                }
                if (entry.getValue().getKind() !=
                    compiler::ir::IrExpressionKind::StringBuild) {
                    throw runtime_error(
                        "Kotlin config packageName must be a String literal."
                    );
                }
                const auto& stringBuild = static_cast<
                    const compiler::ir::IrStringBuildExpression&
                >(entry.getValue());
                string packageName;
                for (const compiler::ir::IrStringSegment& segment :
                     stringBuild.getSegments()) {
                    if (segment.getKind() !=
                        compiler::ir::IrStringSegmentKind::Literal) {
                        throw runtime_error(
                            "Kotlin config packageName cannot interpolate "
                            "symbols."
                        );
                    }
                    packageName += segment.getValue();
                }
                return packageName;
            }
        }

        return nullopt;
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
