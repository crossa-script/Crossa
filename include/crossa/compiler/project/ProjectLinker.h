#pragma once

#include <cstddef>
#include <filesystem>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "crossa/compiler/ast/Declaration.h"
#include "crossa/compiler/ast/SourceUnit.h"
#include "crossa/compiler/source/SourceLocation.h"
#include "crossa/utils/Log.h"

namespace crossa::compiler::project {

// Resolves filename imports into one deterministic project AST.
// link() searches the project tree, detects cycles, and merges declarations.
class ProjectLinker final {
public:
    // Links the entry AST with every transitively imported source file.
    [[nodiscard]] static ast::SourceUnit link(
        const std::filesystem::path& projectRoot,
        const std::filesystem::path& entryPath,
        ast::SourceUnit entrySourceUnit,
        const utils::Log& log
    );

private:
    // Tracks each canonical source path while traversing the import graph.
    enum class VisitState {
        Visiting,
        Visited
    };

    // Creates one linker scoped to a normalized project root.
    ProjectLinker(
        std::filesystem::path projectRoot,
        const utils::Log& log
    );

    // Executes indexing and graph traversal for the entry source.
    [[nodiscard]] ast::SourceUnit run(
        const std::filesystem::path& entryPath,
        ast::SourceUnit entrySourceUnit
    );

    // Indexes every regular .cra file by exact filename.
    void indexProjectSources();

    // Returns whether one source unit declares at least one import.
    [[nodiscard]] static bool hasImports(
        const ast::SourceUnit& sourceUnit
    ) noexcept;

    // Visits one source module and appends its declarations once.
    void visitModule(
        const std::filesystem::path& sourcePath,
        ast::SourceUnit sourceUnit,
        bool entrySource
    );

    // Loads, tokenizes, and parses one imported source module.
    [[nodiscard]] ast::SourceUnit parseImportedSource(
        const std::filesystem::path& sourcePath
    ) const;

    // Resolves one exact import filename or emits a deterministic error.
    [[nodiscard]] std::filesystem::path resolveImport(
        const ast::ImportDeclaration& declaration
    ) const;

    // Ensures the entry source remains inside the selected project root.
    void validateEntryPath(const std::filesystem::path& entryPath) const;

    // Reports one circular dependency using the active traversal chain.
    [[noreturn]] void failCircularImport(
        const std::filesystem::path& targetPath,
        const source::SourceLocation& location
    ) const;

    // Throws one import diagnostic at its original source location.
    [[noreturn]] static void fail(
        const source::SourceLocation& location,
        const std::string& message
    );

    // Returns a stable key for one normalized source path.
    [[nodiscard]] static std::string getPathKey(
        const std::filesystem::path& sourcePath
    );

    std::filesystem::path projectRoot_;
    const utils::Log& log_;
    std::unordered_map<
        std::string,
        std::vector<std::filesystem::path>
    > sourceIndex_;
    std::unordered_map<std::string, VisitState> visitStates_;
    std::vector<std::filesystem::path> activePath_;
    std::vector<std::unique_ptr<ast::Declaration>> linkedDeclarations_;
    std::size_t linkedModuleCount_;
};

}
