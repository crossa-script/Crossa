#pragma once

#include <cstddef>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include "crossa/compiler/ast/SourceUnit.h"
#include "crossa/compiler/ir/Program.h"
#include "crossa/compiler/lexer/Token.h"
#include "crossa/compiler/semantic/TypedSourceUnit.h"
#include "crossa/compiler/source/SourceFile.h"
#include "crossa/utils/Log.h"

namespace crossa::cli {

// Coordinates the Crossa command-line workflow from arguments to execution.
// run() is the entry point while focused helpers own each compiler stage.
class CrossaApplication final {
public:
    // Runs the Crossa command-line application and returns its process status.
    static int run(int argc, char* argv[]);

private:
    // Stores validated command-line options for one Crossa execution.
    struct Arguments final {
        bool debugEnabled;
        std::filesystem::path sourcePath;
    };

    // Parses supported command-line arguments into execution options.
    [[nodiscard]] static std::optional<Arguments> parseArguments(
        int argc,
        char* argv[]
    );

    // Loads, compiles, and executes one Crossa source file.
    static void executeSource(const Arguments& arguments, const utils::Log& log);

    // Compiles an optional sibling config.cra into declarative IR.
    [[nodiscard]] static std::optional<compiler::ir::Program>
    compileSiblingConfiguration(
        const std::filesystem::path& sourcePath,
        const utils::Log& log
    );

    // Ensures a sibling configuration file contains only config declarations.
    static void validateConfigurationProgram(
        const compiler::ir::Program& program
    );

    // Tokenizes one source file and reports the lexer lifecycle.
    [[nodiscard]] static std::vector<compiler::lexer::Token> tokenizeSource(
        const compiler::source::SourceFile& sourceFile,
        const utils::Log& log
    );

    // Parses one token stream and reports the parser lifecycle.
    [[nodiscard]] static compiler::ast::SourceUnit parseSource(
        const std::vector<compiler::lexer::Token>& tokens,
        const compiler::source::SourceFile& sourceFile,
        const utils::Log& log
    );

    // Validates one AST and produces its typed semantic model.
    [[nodiscard]] static compiler::semantic::TypedSourceUnit analyzeSource(
        const compiler::ast::SourceUnit& sourceUnit,
        const compiler::source::SourceFile& sourceFile,
        const utils::Log& log
    );

    // Writes every emitted token when debug logging is enabled.
    static void logTokens(
        const std::vector<compiler::lexer::Token>& tokens,
        const compiler::source::SourceFile& sourceFile,
        const utils::Log& log
    );

    // Writes a concise summary for every parsed AST declaration.
    static void logAst(
        const compiler::ast::SourceUnit& sourceUnit,
        const utils::Log& log
    );

    // Writes a concise summary for every typed semantic declaration.
    static void logSemanticModel(
        const compiler::semantic::TypedSourceUnit& sourceUnit,
        const utils::Log& log
    );

    // Writes a readable summary for every lowered IR instruction.
    static void logIr(
        const compiler::ir::Program& program,
        const utils::Log& log
    );

    // Reports that one numbered Crossa pipeline step has started.
    static void logStepStarted(
        std::size_t step,
        const std::string& name,
        const utils::Log& log
    );

    // Reports that one numbered Crossa pipeline step has completed.
    static void logStepCompleted(
        std::size_t step,
        const std::string& name,
        const utils::Log& log
    );

    inline static constexpr std::size_t TotalSteps = 6;
};

}
