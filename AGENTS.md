# Crossa Agent Instructions

## Responsibility

`AGENTS.md` defines how coding agents operate inside this repository.

`ARCHITECTURE.md` defines how Crossa itself is designed and implemented. It is the primary technical architecture and engineering source of truth.

Before changing architecture, ownership, module boundaries, compiler or runtime behavior, memory, concurrency, ABI, Networking, Android bindings, or iOS bindings, read `ARCHITECTURE.md`. Every implementation must preserve its rules unless an approved ADR explicitly changes them.

`CROSSA_PROJECT_BLUEPRINT.md` provides the broader product direction and roadmap. It must not override the focused technical rules in `ARCHITECTURE.md`; resolve any real contradiction before modifying code.

## Documentation Routing

Read only the documents relevant to the task, starting with the nearest nested `AGENTS.md`:

- Project: `ARCHITECTURE.md`, `docs/project/product.md`, `docs/engineering/engineering-principles.md`
- Compiler: `docs/architecture/compiler.md`, `docs/compiler/ir.md`
- Runtime: `docs/architecture/runtime.md`, `docs/runtime/memory-ownership.md`, `docs/runtime/scheduling.md`
- ABI: `docs/architecture/abi.md`
- Networking: `docs/features/networking.md`
- Language: `docs/language/language-foundation.md` for current `.cra` syntax and semantics; `docs/language/language-roadmap.md` for implementation order and future milestones
- Platforms: `docs/platform/android.md`, `docs/platform/ios.md`
- Decisions: `docs/decisions/`

Some subsystem documents may not exist during the foundation phase. Use `ARCHITECTURE.md`, the blueprint, and existing code as evidence; do not invent missing policy.

## Crossa Language

Crossa uses `.cra` source files. `docs/language/language-foundation.md` is the authoritative specification for supported syntax and semantics. `docs/language/language-roadmap.md` defines implementation order and future compiler/language milestones; planned roadmap items are not supported language features.

Read the language foundation completely and the relevant roadmap sections before modifying:

- `.cra` source loading, tokens, lexer/tokenizer, parser, AST, semantic analysis, symbols, types, diagnostics, compatibility, reserved keywords, or grammar.
- Variables, functions, models, `List<T>`, `print`, `re`, `#identifier` interpolation, execution annotations, `@Sync`, `@Async`, `@AsyncAfter`, or `config.cra`.
- `CrossaRequest`, request-path interpolation, native request lowering, response decoding, language-to-IR lowering, or native `.cra` execution.
- Kotlin/Swift generation, generated async-state APIs, or language-backed Networking behavior.

There is exactly one canonical frontend:

```text
C++ Source Loader
    -> C++ Lexer
    -> C++ Parser
    -> AST
    -> Semantic Analysis
    -> Typed Crossa Representation
    -> Crossa IR
```

Never create Kotlin or Swift `.cra` parsers. C++ owns lexing, parsing, semantic analysis, IR lowering, native execution, scheduler semantics, `CrossaRequest` execution, request construction, networking, response buffering/parsing, and native result state. Kotlin and Swift generators expose platform APIs without redefining language or runtime semantics.

Preserve these current invariants:

- `#identifier` is the only defined interpolation syntax.
- `List<T>` is a built-in collection and does not imply user-defined generics.
- `CrossaRequest` is a compiler/runtime builtin that lowers to native IR; it must not generate separate Kotlin or Swift networking implementations.
- For `@AsyncAfter`, the declared return type is the logical success type and generated APIs preserve `Success(data)` or `Failed(error)` semantics.

Do not invent syntax or semantics while implementing unrelated work. Anything absent from the language foundation is unsupported. This includes nullable syntax, enums, maps, control flow (`if`, `else`, or loops), imports, packages, visibility modifiers, exceptions, extra annotations, user-defined generics, additional HTTP methods or `CrossaRequest` properties, and arbitrary interpolation expressions unless the task intentionally updates the specification.

When a task intentionally changes `.cra` syntax or semantics, update the existing documents under `docs/language/` in the same change. Do not create duplicate language documentation or place it beside compiler source.

## Documentation Location and Maintenance

The root `docs/` directory is the canonical location for Crossa development documentation. Place feature, subsystem, implementation, compiler, runtime, language, translator, generator, workflow, and technical-decision documents somewhere under `docs/`. Never place technical Markdown beside source code.

Only repository-wide documents may remain at the root. The explicit defaults are `README.md`, `AGENTS.md`, and `ARCHITECTURE.md`. Add another root Markdown file only when it is genuinely repository-wide and cannot reasonably live under `docs/`; when uncertain, use `docs/`.

Choose a directory that matches the document's responsibility, for example:

- `docs/architecture/` for detailed architectural views.
- `docs/features/` for feature behavior.
- `docs/compiler/`, `docs/runtime/`, and `docs/language/` for subsystem concepts.
- `docs/platform/` for Android and iOS integration.
- `docs/decisions/` for ADRs.
- `docs/development/` for build, tooling, and contributor workflows.

Before creating documentation, search `docs/` for the same concept and update the existing document when appropriate. Create a new file only for a separate responsibility. Use descriptive lowercase kebab-case names such as `semantic-analysis.md` or `memory-ownership.md`; avoid vague names such as `notes.md`, `details.md`, or `misc.md`.

Documentation must follow the implementation. Do not describe hypothetical behavior as implemented; label future material `Planned`, `Proposed`, or `Experimental`. Update documentation when a change materially affects compiler stages, IR, generation, language behavior, runtime ownership, scheduling, Networking, serialization, platform bindings, ABI, adapters, build or CLI behavior, public APIs, security, or significant performance techniques. Do not document trivial details already clear from code.

Keep responsibilities distinct: `AGENTS.md` explains how agents work, `ARCHITECTURE.md` defines global engineering rules, and `docs/` explains individual concepts and subsystems. Reference the architecture instead of duplicating large sections of it.

## Before Editing

1. Inspect the affected subsystem, tests, build targets, and dependency direction.
2. Read `ARCHITECTURE.md`, the nearest `AGENTS.md`, and relevant subsystem documents.
3. Identify ABI, IR, ownership, lifetime, concurrency, security, performance, binary-size, and platform implications.
4. Check whether the change requires an ADR.
5. Select the smallest complete change that satisfies the task without speculative infrastructure.

Do not implement unrelated cleanup or future features. Implement only `.cra` behavior defined by the language foundation and required by the task.

## While Editing

- Follow established local naming, formatting, file, namespace, include, and test conventions.
- Preserve the architecture and keep generated output deterministic.
- Do not add a dependency without the review required by `ARCHITECTURE.md`.
- Keep changes focused and do not silently introduce a new architectural pattern.
- Preserve user changes and avoid rewriting unrelated files.

## Strict Development Rules

- Put a concise class summary immediately above every class declaration. The summary must explain the class responsibility and name its most important functions.
- Put a concise one- or two-line function comment immediately above every function declaration and definition, never inside the function body.
- Keep comments factual, short, and focused on responsibility or constraints. Do not add implementation-narration comments inside function bodies.
- Use four spaces for every indentation level in C++ source files. Never use tabs, and indent nested blocks, continued parameters, and initializer lists consistently.
- Separate workflows into focused, human-readable functions, each performing one logical operation.
- Use lower camel case for variables and Pascal case for static variables.
- Keep shared stateless functions as static members of a focused utility class such as `UrlUtils` or `BufferUtils`.
- Never declare free utility functions or static variables at namespace or global scope; give them a parent class.
- In `.cpp` files, use `using namespace std;` when standard-library names are required instead of repeating `std::` qualifiers. Never place it in a public header.
- Never call `std::cout` from application code. Call `PrintUtils::println(string)`; only its implementation may call `std::cout`.
- Apply these rules strictly unless the user explicitly requests a documented exception.

## Validation and Handoff

1. Format every changed source file with the repository formatter.
2. Build affected targets and run the smallest relevant test set.
3. Run applicable sanitizers and benchmarks for ownership, concurrency, or performance-sensitive work.
4. Recheck cancellation, errors, bounds, lifetimes, allocations, copies, boundary crossings, and ABI/IR compatibility.
5. Update architecture or ADR documentation when the implementation changes a documented decision.
6. Report changed files, validation performed, and any checks that could not be run.

Never claim a command or test succeeded unless it was executed. The repository currently has no committed executable build configuration; do not invent build or test commands until those targets exist.

## Architecture Changes

Changes to ABI, IR, ownership, scheduler, transport, parsing, native object or memory layout, module boundaries, platform interoperability, or major dependencies require an ADR under `docs/decisions/`. Do not prematurely lock decisions that `ARCHITECTURE.md` reserves for benchmarks.
