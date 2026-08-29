# ADR 0003: CRA Filename Import Graph

## Status

Accepted.

## Context

Crossa projects need to separate models, request functions, repositories, and other declarations across nested directories. Compiling every `.cra` file independently prevents typed references and function calls from crossing source-file boundaries. A path-based import would also couple source code to the current directory layout and make refactoring directories unnecessarily expensive.

## Decision

- `import #filename.cra#` is the only import syntax in this language version.
- Imports appear before all declarations and identify an exact filename, not a relative or absolute path.
- The CLI project root is the normalized current working directory. The entry source must be inside that root.
- The C++ project linker recursively indexes regular, non-symlink `.cra` files under the project root.
- Zero filename matches produce a missing-import diagnostic. Multiple exact matches produce an ambiguity diagnostic listing every candidate; traversal order never selects a winner.
- Imports form a directed graph. Dependencies are linked before importers, cycles are diagnosed with their chain, and each canonical source path is linked once.
- All declarations in the reachable graph share one project symbol namespace. This allows imported models, variables, functions, and request functions to reference each other through the linked semantic model.
- Imported files contain declarations only. Top-level expressions execute only from the CLI entry file, preventing direct imported startup or network calls. Imported source-variable initializers retain normal declaration semantics and run once in dependency order.
- `config.cra` remains reserved runtime configuration and cannot be imported.
- Source locations retain their original file paths after linking so later semantic diagnostics identify the correct imported file.

## Alternatives

- Relative path imports were rejected for this version because the requested filename-based project lookup should survive directory moves.
- Selecting the first recursive filename match was rejected because filesystem traversal order is not a stable language rule.
- Textually copying imported source was rejected because it obscures cycles, duplicates declarations in diamond graphs, and loses source ownership.
- Executing top-level expressions from imported files was rejected because importing declarations must not trigger hidden network or background work.
- Parsing imports in Kotlin or Swift was rejected because C++ remains the only canonical `.cra` frontend.

## Consequences

- Imported filenames must be unique anywhere under one project root when referenced.
- Directory structure can change without updating import statements as long as imported filenames remain unique.
- The linked declaration order is deterministic and dependency-first.
- Future package or visibility syntax requires a separate language decision; it is not implied by filename imports.
- Project tools should execute Crossa from the intended project root so recursive resolution uses the correct source set.

## Compatibility and Migration

`import` becomes a reserved keyword and raw hash-delimited text outside string literals is interpreted as an import filename token. Existing files without imports preserve their compilation and execution behavior. Projects that previously used `import` as an identifier must rename that symbol.
