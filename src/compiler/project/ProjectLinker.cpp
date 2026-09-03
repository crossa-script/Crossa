#include "crossa/compiler/project/ProjectLinker.h"

#include <algorithm>
#include <stdexcept>
#include <system_error>
#include <utility>

#include "crossa/compiler/CompilerResourceLimits.h"

#include "crossa/compiler/ast/AstPrinter.h"
#include "crossa/compiler/lexer/Lexer.h"
#include "crossa/compiler/lexer/TokenType.h"
#include "crossa/compiler/parser/Parser.h"
#include "crossa/compiler/source/SourceFile.h"
#include "crossa/compiler/source/SourceLoader.h"

using namespace std;

namespace crossa::compiler::project {

    // Links the entry AST with every transitively imported source file.
    ast::SourceUnit ProjectLinker::link(
        const filesystem::path& projectRoot,
        const filesystem::path& entryPath,
        ast::SourceUnit entrySourceUnit,
        const utils::Log& log
    ) {
        error_code error;
        const filesystem::path normalizedRoot =
            filesystem::weakly_canonical(projectRoot, error);
        if (error || !filesystem::is_directory(normalizedRoot)) {
            throw runtime_error(
                "Unable to resolve Crossa project root: " +
                projectRoot.string()
            );
        }

        ProjectLinker linker(normalizedRoot, log);
        return linker.run(entryPath, std::move(entrySourceUnit));
    }

    // Creates one linker scoped to a normalized project root.
    ProjectLinker::ProjectLinker(
        filesystem::path projectRoot,
        const utils::Log& log
    )
        : projectRoot_(std::move(projectRoot)),
          log_(log),
          linkedModuleCount_(0),
          importDepth_(0),
          indexedSourceCount_(0),
          indexedSourceBytes_(0) {}

    // Executes indexing and graph traversal for the entry source.
    ast::SourceUnit ProjectLinker::run(
        const filesystem::path& entryPath,
        ast::SourceUnit entrySourceUnit
    ) {
        error_code error;
        const filesystem::path normalizedEntry =
            filesystem::weakly_canonical(entryPath, error);
        if (error) {
            throw runtime_error(
                "Unable to resolve Crossa entry source: " +
                entryPath.string()
            );
        }

        validateEntryPath(normalizedEntry);
        log_.debug("Project root selected: " + projectRoot_.string());
        if (hasImports(entrySourceUnit)) {
            indexProjectSources();
        } else {
            log_.debug("Entry source has no imports; project indexing skipped");
        }
        visitModule(normalizedEntry, std::move(entrySourceUnit), true);
        log_.debug(
            "Project linking completed: " +
            to_string(linkedModuleCount_) + " source modules, " +
            to_string(linkedDeclarations_.size()) + " declarations"
        );
        return ast::SourceUnit(std::move(linkedDeclarations_));
    }

    // Returns whether one source unit declares at least one import.
    bool ProjectLinker::hasImports(
        const ast::SourceUnit& sourceUnit
    ) noexcept {
        for (const unique_ptr<ast::Declaration>& declaration :
             sourceUnit.getDeclarations()) {
            if (declaration->getKind() == ast::DeclarationKind::Import) {
                return true;
            }
        }
        return false;
    }

    // Indexes every regular .cra file by exact filename.
    void ProjectLinker::indexProjectSources() {
        log_.debug("Project source indexing started");
        error_code error;
        filesystem::recursive_directory_iterator iterator(
            projectRoot_,
            filesystem::directory_options::skip_permission_denied,
            error
        );
        const filesystem::recursive_directory_iterator end;

        while (!error && iterator != end) {
            const filesystem::directory_entry& entry = *iterator;
            error_code entryError;
            const bool isSymlink = entry.is_symlink(entryError);
            const bool isRegularFile = !entryError &&
                entry.is_regular_file(entryError);
            if (!entryError && !isSymlink && isRegularFile &&
                entry.path().extension() == ".cra") {
                if (indexedSourceCount_ >=
                    CompilerResourceLimits::MaximumProjectSourceFiles) {
                    throw runtime_error(
                        "Crossa project contains more source files than the configured limit."
                    );
                }
                if (indexedSourceBytes_ >=
                    CompilerResourceLimits::MaximumProjectSourceBytes) {
                    throw runtime_error(
                        "Crossa project source files exceed the configured byte limit."
                    );
                }
                const uintmax_t fileBytes = entry.file_size(entryError);
                if (entryError || fileBytes >
                    CompilerResourceLimits::MaximumProjectSourceBytes ||
                    indexedSourceBytes_ >
                        CompilerResourceLimits::MaximumProjectSourceBytes -
                        static_cast<size_t>(fileBytes)) {
                    throw runtime_error(
                        "Crossa project source files exceed the configured byte limit."
                    );
                }
                indexedSourceBytes_ += static_cast<size_t>(fileBytes);
                ++indexedSourceCount_;
                const filesystem::path normalizedPath =
                    filesystem::weakly_canonical(entry.path(), entryError);
                if (!entryError) {
                    sourceIndex_[entry.path().filename().string()].push_back(
                        normalizedPath
                    );
                }
            }

            iterator.increment(error);
            if (error) {
                log_.debug(
                    "Project source indexing skipped an unreadable entry: " +
                    error.message()
                );
                error.clear();
            }
        }

        for (auto& indexedSources : sourceIndex_) {
            vector<filesystem::path>& paths = indexedSources.second;
            sort(
                paths.begin(),
                paths.end(),
                [](const filesystem::path& left,
                   const filesystem::path& right) {
                    return left.generic_string() < right.generic_string();
                }
            );
        }
        log_.debug(
            "Project source indexing completed: " +
            to_string(sourceIndex_.size()) + " unique filenames"
        );
    }

    // Visits one source module and appends its declarations once.
    void ProjectLinker::visitModule(
        const filesystem::path& sourcePath,
        ast::SourceUnit sourceUnit,
        bool entrySource
    ) {
        if (linkedModuleCount_ >=
            CompilerResourceLimits::MaximumImportedModules) {
            fail(
                source::SourceLocation(sourcePath.string(), 1, 1),
                "Imported module count exceeded the configured limit."
            );
        }
        if (importDepth_ >= CompilerResourceLimits::MaximumImportDepth) {
            fail(
                source::SourceLocation(sourcePath.string(), 1, 1),
                "Import depth exceeded the configured limit."
            );
        }
        const string pathKey = getPathKey(sourcePath);
        const auto existingState = visitStates_.find(pathKey);
        if (existingState != visitStates_.end()) {
            if (existingState->second == VisitState::Visited) {
                log_.debug("Import already linked: " + sourcePath.string());
                return;
            }
            failCircularImport(
                sourcePath,
                source::SourceLocation(sourcePath.string(), 1, 1)
            );
        }

        visitStates_.emplace(pathKey, VisitState::Visiting);
        activePath_.push_back(sourcePath);
        ++importDepth_;
        log_.debug("Linking source module: " + sourcePath.string());

        vector<unique_ptr<ast::Declaration>> declarations =
            sourceUnit.takeDeclarations();
        for (const unique_ptr<ast::Declaration>& declaration : declarations) {
            if (declaration->getKind() != ast::DeclarationKind::Import) {
                continue;
            }

            const auto& import =
                static_cast<const ast::ImportDeclaration&>(*declaration);
            const filesystem::path importedPath = resolveImport(import);
            const auto importedState = visitStates_.find(
                getPathKey(importedPath)
            );
            if (importedState != visitStates_.end() &&
                importedState->second == VisitState::Visiting) {
                failCircularImport(importedPath, import.getLocation());
            }
            if (importedState != visitStates_.end() &&
                importedState->second == VisitState::Visited) {
                log_.debug(
                    "Import reused from linked graph: " +
                    importedPath.string()
                );
                continue;
            }

            log_.debug(
                "Import resolved: " + import.getFilename() + " -> " +
                importedPath.string()
            );
            ast::SourceUnit importedSourceUnit =
                parseImportedSource(importedPath);
            visitModule(
                importedPath,
                std::move(importedSourceUnit),
                false
            );
        }

        for (unique_ptr<ast::Declaration>& declaration : declarations) {
            if (declaration->getKind() == ast::DeclarationKind::Import) {
                continue;
            }
            if (!entrySource &&
                declaration->getKind() == ast::DeclarationKind::Config) {
                fail(
                    declaration->getLocation(),
                    "config declarations cannot be imported; config.cra "
                    "remains project runtime configuration."
                );
            }
            if (!entrySource &&
                declaration->getKind() == ast::DeclarationKind::Expression) {
                fail(
                    declaration->getLocation(),
                    "Imported files may contain declarations only; "
                    "top-level execution belongs to the entry file."
                );
            }
            linkedDeclarations_.push_back(std::move(declaration));
        }

        activePath_.pop_back();
        --importDepth_;
        visitStates_[pathKey] = VisitState::Visited;
        ++linkedModuleCount_;
        log_.debug("Source module linked: " + sourcePath.string());
    }

    // Loads, tokenizes, and parses one imported source module.
    ast::SourceUnit ProjectLinker::parseImportedSource(
        const filesystem::path& sourcePath
    ) const {
        source::SourceFile sourceFile =
            source::SourceLoader::load(sourcePath, log_);
        log_.debug("Imported lexer started: " + sourcePath.string());
        lexer::Lexer lexer(sourceFile);
        const vector<lexer::Token> tokens = lexer.tokenize();
        for (const lexer::Token& token : tokens) {
            log_.debug(
                "Imported token " +
                string(lexer::TokenTypeUtils::toString(token.getType())) +
                " '" + string(token.getLexeme(sourceFile)) + "' at " +
                sourcePath.string() + ":" + to_string(token.getLine()) +
                ":" + to_string(token.getColumn())
            );
        }
        log_.debug(
            "Imported lexer completed: " + to_string(tokens.size()) +
            " tokens"
        );

        log_.debug("Imported parser started: " + sourcePath.string());
        parser::Parser parser(tokens, sourceFile);
        ast::SourceUnit sourceUnit = parser.parse();
        const vector<string> summaries = ast::AstPrinter::summarize(sourceUnit);
        for (const string& summary : summaries) {
            log_.debug(sourcePath.string() + " " + summary);
        }
        log_.debug(
            "Imported parser completed: " +
            to_string(sourceUnit.getDeclarations().size()) +
            " declarations"
        );
        return sourceUnit;
    }

    // Resolves one exact import filename or emits a deterministic error.
    filesystem::path ProjectLinker::resolveImport(
        const ast::ImportDeclaration& declaration
    ) const {
        if (declaration.getFilename() == "config.cra") {
            fail(
                declaration.getLocation(),
                "config.cra cannot be imported."
            );
        }

        const auto indexedSources = sourceIndex_.find(
            declaration.getFilename()
        );
        if (indexedSources == sourceIndex_.end() ||
            indexedSources->second.empty()) {
            fail(
                declaration.getLocation(),
                "Imported source '" + declaration.getFilename() +
                "' was not found under project root '" +
                projectRoot_.string() + "'."
            );
        }
        if (indexedSources->second.size() > 1) {
            string message =
                "Imported source '" + declaration.getFilename() +
                "' is ambiguous. Matching files:";
            for (const filesystem::path& candidate : indexedSources->second) {
                message += "\n  " + candidate.string();
            }
            fail(declaration.getLocation(), message);
        }

        return indexedSources->second.front();
    }

    // Ensures the entry source remains inside the selected project root.
    void ProjectLinker::validateEntryPath(
        const filesystem::path& entryPath
    ) const {
        const filesystem::path relativePath =
            entryPath.lexically_relative(projectRoot_);
        if (relativePath.empty() || relativePath.is_absolute() ||
            *relativePath.begin() == "..") {
            throw runtime_error(
                "Crossa entry source must be inside project root '" +
                projectRoot_.string() + "'."
            );
        }
    }

    // Reports one circular dependency using the active traversal chain.
    [[noreturn]] void ProjectLinker::failCircularImport(
        const filesystem::path& targetPath,
        const source::SourceLocation& location
    ) const {
        string chain;
        auto cycleStart = find(
            activePath_.begin(),
            activePath_.end(),
            targetPath
        );
        if (cycleStart == activePath_.end()) {
            cycleStart = activePath_.begin();
        }
        for (auto current = cycleStart; current != activePath_.end(); ++current) {
            if (!chain.empty()) {
                chain += " -> ";
            }
            chain += current->filename().string();
        }
        if (!chain.empty()) {
            chain += " -> ";
        }
        chain += targetPath.filename().string();
        fail(location, "Circular import detected: " + chain + ".");
    }

    // Throws one import diagnostic at its original source location.
    [[noreturn]] void ProjectLinker::fail(
        const source::SourceLocation& location,
        const string& message
    ) {
        const string diagnosticPath = location.getSourcePath().empty()
            ? "<unknown>"
            : string(location.getSourcePath());
        throw runtime_error(
            diagnosticPath + ":" + to_string(location.getLine()) + ":" +
            to_string(location.getColumn()) + ": CRA1002 " + message
        );
    }

    // Returns a stable key for one normalized source path.
    string ProjectLinker::getPathKey(
        const filesystem::path& sourcePath
    ) {
        return sourcePath.generic_string();
    }

}
