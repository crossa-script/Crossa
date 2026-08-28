# Crossa Language Roadmap

> **Status:** Active language roadmap  
> **Source extension:** `.cra`  
> **Primary implementation:** C++20  
> **Scope:** Crossa language frontend, semantic model, IR lowering, native execution, Kotlin generation, Swift generation, and native networking expressions.

---

## 1. Roadmap Purpose

This roadmap defines the order in which the Crossa `.cra` language should become real.

The roadmap is intentionally vertical-slice driven. Crossa should not build a huge parser, type system, or runtime before proving that a small `.cra` source file can move through the complete pipeline:

```text
.cra source
    ->
C++ lexer
    ->
C++ parser
    ->
AST
    ->
semantic analysis
    ->
typed Crossa representation / IR
    ->
native execution or target generation
```

The language must remain small, deterministic, and native-first.

The first production goal is not language complexity.

The first production goal is:

> A simple `.cra` source file is parsed once in C++, validated once, lowered once, then either executed by the Crossa C++ runtime or used to generate deterministic Android and iOS APIs.

---

## 2. Language Direction

Crossa is a scripting and code-generation language, not a general-purpose programming language.

Initial language concepts:

```text
fun
re
var
print
model
config

@Sync
@Async
@AsyncAfter

Int
String
Bool
List<T>

CrossaRequest
```

The language should grow only when a real Crossa capability requires new syntax.

Do not add features merely because Kotlin, Swift, C++, Java, or JavaScript provide them.

---

## 3. Initial Syntax Targets

### Function

```cra
fun add(a: Int, b: Int): Int {
    re a + b
}
```

### Variable

```cra
var name: String = "ahmad"
```

### String Interpolation

```cra
var name: String = "ahmad"
print("Name is : #name")
```

Inside a string, `#` immediately followed by a valid identifier interpolates the current value of that variable or parameter.

### Model

```cra
model User(
    id: Int,
    name: String,
    isActivated: Bool
)
```

### Async Native Request

```cra
model User(
    id: Int
)

@AsyncAfter
fun getUsers(id: Int): List<User> {
    re CrossaRequest {
        path: "/v1/users",
        method: GET
    }
}
```

A more useful path can consume the function parameter through the same interpolation system:

```cra
@AsyncAfter
fun getUser(id: Int): User {
    re CrossaRequest {
        path: "/v1/users/#id",
        method: GET
    }
}
```

`CrossaRequest` is a built-in native networking expression. It is not a user-defined class.

---

# Phase 0 — Language Contract

## Goal

Freeze the first intentional language surface before implementation expands.

## Define

- `.cra` extension.
- Source-unit identity.
- Reserved keywords.
- Built-in types.
- `List<T>`.
- Function syntax.
- Variables.
- `re`.
- `print`.
- Models.
- String interpolation using `#identifier`.
- `@Sync`.
- `@Async`.
- `@AsyncAfter`.
- `config.cra`.
- `CrossaRequest`.
- Initial networking request fields.
- Async completion-state semantics.

## Deliverable

`docs/language/language-foundation.md` is the language source of truth.

---

# Phase 1 — Source and Diagnostic Foundation

## Goal

Create the lowest-level source infrastructure used by every compiler stage.

## Build

- `SourceFile`.
- `SourceLocation`.
- `SourceRange`.
- Source-buffer ownership.
- UTF-8 source policy.
- Diagnostic representation.
- Diagnostic code/category.
- Diagnostic sink/reporting path.

## Engineering Rules

- Source ranges should avoid copying source text.
- Diagnostics must preserve file, line, and column.
- Malformed input must fail safely.
- Compiler internals must not throw raw parser failures to users.

## Deliverable

Crossa can load a `.cra` file and report deterministic source-aware diagnostics.

---

# Phase 2 — Lexer / Tokenizer

## Goal

Tokenize the initial `.cra` language in C++.

## Tokens

Initial token families include:

```text
Identifiers
Integer literals
String literals
Boolean literals

fun
re
var
print
model
config

@Sync
@Async
@AsyncAfter

Int
String
Bool
List
CrossaRequest
GET

(
)
{
}
<
>
:
,
=
+
-
*
/
#
EOF
```

String interpolation can be represented through dedicated interpolation-aware string tokens or through a string-literal representation containing parsed segments.

The lexer must not copy every source token unnecessarily.

## Deliverable

All Phase-0 syntax can be tokenized deterministically.

---

# Phase 3 — Parser and AST

## Goal

Parse the initial language into a syntax-only AST.

## Build AST support for

- Source units.
- Function declarations.
- Parameters.
- Return types.
- Variable declarations.
- Return statements.
- Function calls.
- Arithmetic expressions.
- Print calls/builtin calls.
- Models.
- Model fields.
- Type references.
- `List<T>`.
- Execution annotations.
- Config blocks.
- `CrossaRequest`.
- Request properties.
- Interpolated strings.

## Important Rule

AST nodes represent syntax.

Do not place:

- Kotlin generation behavior.
- Swift generation behavior.
- JNI behavior.
- HTTP execution.
- native scheduler implementation.

inside AST nodes.

## Deliverable

The syntax examples in this roadmap parse into stable AST structures.

---

# Phase 4 — Initial Type System

## Goal

Introduce a small deterministic language type system.

## Types

Initial built-in scalar types:

```text
Int
String
Bool
```

Initial built-in collection type:

```text
List<T>
```

Examples:

```cra
List<Int>
List<String>
List<User>
```

`List<T>` is a built-in parameterized collection type.

This does not mean Crossa supports general user-defined generics.

## Model Types

Models become valid named types:

```cra
model User(
    id: Int
)
```

allows:

```cra
fun getUsers(): List<User>
```

## Deliverable

Crossa resolves scalars, models, and `List<T>` consistently across all targets.

---

# Phase 5 — Semantic Analysis

> **Implementation status:** Implemented for the current pure-language parser surface, including resolved types, scopes, calls, returns, models, config, execution policies, and interpolation. `CrossaRequest` semantic validation remains deferred with its parser/runtime milestone.

## Goal

Turn syntax into validated typed meaning.

## Validate

- Duplicate symbols.
- Unknown symbols.
- Unknown types.
- Parameter types.
- Variable types.
- Function argument counts.
- Function argument types.
- Function return types.
- `List<T>` element types.
- Model fields.
- Model/file identity.
- Execution annotation combinations.
- Config keys.
- Request fields.
- Request return type.
- Interpolation identifiers.

## String Interpolation Validation

Given:

```cra
var name: String = "ahmad"
print("Name is : #name")
```

`name` must resolve in the current lexical/function/source scope.

Unknown interpolation variables must produce a compile diagnostic.

## Deliverable

Generators and runtime backends receive already validated typed input.

---

# Phase 6 — Typed IR Foundation

> **Implementation status:** The first platform-neutral lowering pass is implemented for the current pure-language semantic model. Runtime-backed `CrossaRequest` lowering remains deferred.

## Goal

Lower valid language semantics into platform-neutral Crossa IR.

## IR concepts

- Source unit.
- Function.
- Parameter.
- Local/source variable.
- Scalar constant.
- Binary expression.
- Return.
- Model.
- Model field.
- List type.
- String interpolation plan.
- Execution policy.
- Native request operation.
- Request path plan.
- Request method.
- Expected response type.

## Example

Source:

```cra
@AsyncAfter
fun getUsers(id: Int): List<User> {
    re CrossaRequest {
        path: "/v1/users",
        method: GET
    }
}
```

should lower conceptually to:

```text
Function
  name = getUsers
  params = [id: Int]
  logicalResultType = List<User>
  executionPolicy = AsyncAfter
  body =
    Return
      NativeRequest
        path = "/v1/users"
        method = GET
        responseType = List<User>
```

The IR must not depend on Kotlin or Swift.

---

# Phase 7 — Native Expression Execution

## Goal

Execute simple non-network `.cra` expressions through C++.

## First vertical slice

```cra
fun add(a: Int, b: Int): Int {
    re a + b
}
```

Pipeline:

```text
source
  ->
lexer
  ->
parser
  ->
AST
  ->
semantic model
  ->
IR
  ->
native execution
  ->
3
```

## Add

- Int values.
- Bool values.
- String values.
- Arithmetic.
- Variables.
- Function calls.
- `re`.
- `print`.

## Deliverable

Simple Crossa scripts can execute without Kotlin or Swift.

---

# Phase 8 — String Interpolation

## Goal

Implement `#identifier` interpolation inside Crossa strings.

## Syntax

```cra
var name: String = "ahmad"
print("Name is : #name")
```

Result:

```text
Name is : ahmad
```

Parameters are also valid:

```cra
fun greet(name: String) {
    print("Hello #name")
}
```

## V0 Rules

Supported:

```text
#name
#userId
#count
```

Not initially supported:

```text
#{name}
#user.name
#items[0]
#function()
```

Those require separate language design.

## Internal Representation

Do not repeatedly parse interpolation at runtime.

The lexer/parser should turn an interpolated string into segments such as:

```text
Literal("Name is : ")
Variable(name)
```

The compiler can lower these segments into an efficient string-building plan.

## Deliverable

Native execution and target generators preserve identical interpolation semantics.

---

# Phase 9 — Models

## Goal

Make models first-class typed declarations.

## Syntax

```cra
model User(
    id: Int,
    name: String,
    isActivated: Bool
)
```

## Build

- Model symbols.
- Field symbols.
- Field-order preservation.
- Model type references.
- Model IR.
- Native model representation foundation.
- Kotlin representation.
- Swift representation.

## Deliverable

`User.cra` generates/represents `User` consistently in native, Android, and iOS targets.

---

# Phase 10 — Kotlin and Swift Pure-Code Generators

## Goal

Generate deterministic platform source for the supported pure-language subset.

## Android

Example:

```text
UsersController.cra
    ->
UsersController.kt
```

## iOS

```text
UsersController.cra
    ->
UsersController.swift
```

## Rules

- Generators consume typed IR.
- They never parse `.cra`.
- Generated naming is deterministic.
- Generated semantics match native execution.
- Generated files are not source-of-truth files.

## Deliverable

A pure Crossa function can execute natively and generate equivalent Kotlin and Swift.

---

# Phase 11 — Shared Scheduler and Execution Policies

## Goal

Implement scheduling semantics for:

```text
@Sync
@Async
@AsyncAfter
```

## `@Sync`

Executes synchronously in the current Crossa execution context.

## `@Async`

Schedules execution through the shared Crossa scheduler.

It does not create an unbounded new OS thread per call.

## `@AsyncAfter`

Schedules execution and reports a terminal state back to the caller.

Logical source return type:

```cra
List<User>
```

Generated completion payload:

```text
Success(List<User>)
or
Failed(CrossaError)
```

## Deliverable

Execution policies are native scheduler semantics, not platform-specific reimplementations.

---

# Phase 12 — Async Completion State

## Goal

Define the generated caller-facing state contract for `@AsyncAfter`.

Source:

```cra
@AsyncAfter
fun getUsers(id: Int): List<User> {
    ...
}
```

does not expose a synchronous `List<User>` return to Android/iOS.

Its source return type is the **logical success type**.

The generated platform API takes a completion lambda/closure whose state contains either success data or failure information.

## Semantic State

Conceptually:

```text
CrossaState<T>
    |
    +-- Success<T>
    |      data: T
    |
    +-- Failed
           error: CrossaError
```

## Android Concept

```kotlin
sealed interface CrossaState<out T> {

    data class Success<T>(
        val data: T
    ) : CrossaState<T>

    data class Failed(
        val error: CrossaError
    ) : CrossaState<Nothing>
}
```

Generated function concept:

```kotlin
fun getUsers(
    id: Int,
    onState: (CrossaState<List<User>>) -> Unit
)
```

## iOS Concept

Swift should expose the same two semantic states using an idiomatic Swift representation, for example an enum or equivalent generated wrapper:

```swift
enum CrossaState<T> {
    case success(T)
    case failed(CrossaError)
}
```

Generated function concept:

```swift
func getUsers(
    id: Int,
    onState: @escaping (CrossaState<[User]>) -> Void
)
```

Exact public target naming may be finalized by platform-generator documentation, but the semantic state is fixed:

```text
Success(data)
Failed(error)
```

## Runtime Rules

- Completion is delivered exactly once.
- Native errors are never swallowed.
- Native state remains valid until delivery completes.
- Runtime shutdown must not cause unsafe callbacks.
- Platform code does not re-execute the `.cra` function.
- Completion transport is a thin binding over native state.

## Deliverable

`@AsyncAfter` has one deterministic cross-platform meaning.

---

# Phase 13 — Networking Configuration

## Goal

Implement `config.cra` for native networking configuration.

Example:

```cra
config {
    baseUrl: "https://api.example.com",
    timeoutRequest: 3000,
    interceptor: true
}
```

## Initial Keys

```text
baseUrl: String
timeoutRequest: Int
interceptor: Bool
```

`timeoutRequest` is measured in milliseconds.

Unknown and duplicate keys are compile errors.

## Deliverable

The native Network module consumes validated config semantics.

---

# Phase 14 — `CrossaRequest` Foundation

## Goal

Introduce native network requests as a language builtin.

## Syntax

```cra
re CrossaRequest {
    path: "/v1/users",
    method: GET
}
```

`CrossaRequest` is not instantiated as an ordinary model/class.

It lowers directly to a native request operation in Crossa IR.

## Initial Request Fields

Required:

```text
path
method
```

Initial method required by the foundation:

```text
GET
```

Additional methods should be added through the networking/language specification when implemented.

## Return-Type Driven Decoding

The containing function's logical return type defines the expected response type.

Example:

```cra
@AsyncAfter
fun getUsers(id: Int): List<User> {
    re CrossaRequest {
        path: "/v1/users",
        method: GET
    }
}
```

means:

```text
HTTP response bytes
    ->
C++ native response buffer
    ->
generated/schema-aware C++ parsing
    ->
List<User> native result
    ->
Success(List<User>)
```

The return model is not parsed by Kotlin or Swift.

## Deliverable

A request can be represented in typed IR and executed by the native network runtime.

---

# Phase 15 — Request String Interpolation

## Goal

Reuse language string interpolation inside networking paths.

Example:

```cra
@AsyncAfter
fun getUser(id: Int): User {
    re CrossaRequest {
        path: "/v1/users/#id",
        method: GET
    }
}
```

The compiler resolves `#id` against the function parameter.

The request encoder uses the resulting typed interpolation plan.

The runtime should not scan raw path strings looking for `#` on every call.

Conceptually:

```text
"/v1/users/#id"
    ->
compile-time interpolation plan
    ->
StaticSegment("/v1/users/")
ParameterSegment(id)
```

At runtime, the request encoder writes the path efficiently.

## Deliverable

Path interpolation works through the same language semantics as ordinary strings without runtime source parsing.

---

# Phase 16 — Native Response Decoding

## Goal

Turn response bytes directly into the logical `.cra` return type.

Example:

```cra
model User(
    id: Int
)

@AsyncAfter
fun getUsers(id: Int): List<User> {
    re CrossaRequest {
        path: "/v1/users",
        method: GET
    }
}
```

Expected native pipeline:

```text
libcurl/native transport
    ->
native response buffer
    ->
generated C++ decoder
    ->
native List<User>
    ->
CrossaState<List<User>>
    ->
thin Android/iOS completion bridge
```

## Performance Rules

- No Kotlin JSON parsing.
- No Swift JSON parsing.
- No generic platform DTO parsing.
- Avoid generic JSON DOM when schema-specific parsing is available.
- Avoid eager full managed object duplication for large collections.
- Keep response ownership explicit.
- Keep boundary crossings coarse.

## Deliverable

The first Crossa request returns a typed native model/list through `@AsyncAfter`.

---

# Phase 17 — Generated Networking APIs

## Goal

Generate ergonomic Android and iOS functions from native request declarations.

Input:

```cra
model User(
    id: Int
)

@AsyncAfter
fun getUsers(id: Int): List<User> {
    re CrossaRequest {
        path: "/v1/users",
        method: GET
    }
}
```

Android concept:

```kotlin
fun getUsers(
    id: Int,
    onState: (CrossaState<List<User>>) -> Unit
)
```

iOS concept:

```swift
func getUsers(
    id: Int,
    onState: @escaping (CrossaState<[User]>) -> Void
)
```

Implementation is a thin bridge over the native Crossa request operation.

## Deliverable

The application calls generated Kotlin/Swift code while transport, parsing, state, and data originate from C++.

---

# Phase 18 — Additional Request Capabilities

## Goal

Expand `CrossaRequest` only as the native network module becomes ready.

Potential future request properties:

```text
query
headers
body
timeout
auth
multipart
download
retry
```

Potential HTTP methods:

```text
POST
PUT
PATCH
DELETE
HEAD
```

These are planned directions, not implicitly supported syntax until documented and implemented.

---

# Phase 19 — Project Compilation and CLI Integration

## Goal

Compile a Crossa source set rather than isolated files.

Example:

```text
config.cra
User.cra
UsersController.cra
```

The compiler:

1. Loads project sources.
2. Tokenizes/parses them.
3. Builds the project symbol table.
4. Performs semantic analysis.
5. Lowers to unified IR.
6. Links required native runtime modules.
7. Selects execution/generation targets.
8. Emits deterministic artifacts.

## Deliverable

A Crossa project can generate native runtime artifacts plus Android and iOS APIs.

---

# Phase 20 — Performance Hardening

## Goal

Optimize only after the complete pipeline exists.

Measure:

- Lexer allocations.
- Parser allocations.
- Semantic-analysis allocations.
- IR memory.
- String interpolation overhead.
- Request path-building overhead.
- Network buffer copies.
- Response parse time.
- Native model construction.
- Async-state delivery.
- JNI crossings.
- Swift/native crossings.
- Android managed allocations.
- iOS materialization cost.
- Binary size.

Do not claim Crossa is faster because it is implemented in C++.

Measure the complete path.

---

# Phase 21 — Language Growth Gate

Before adding larger language features, require a concrete Crossa use case.

Candidates may eventually include:

```text
nullable types
additional collections
enums
if / else
error-handling syntax
imports
visibility
request bodies
headers/query declarations
WebSocket operations
database operations
```

Do not add:

```text
inheritance
general-purpose generics
reflection
metaprogramming
eval
dynamic objects
```

without a clear architectural requirement.

---

# Current Language Milestones

## Milestone A — Pure Function

```cra
fun add(a: Int, b: Int): Int {
    re a + b
}
```

Must:

- parse,
- validate,
- lower,
- execute natively,
- generate Kotlin,
- generate Swift.

---

## Milestone B — Interpolated String

```cra
var name: String = "ahmad"
print("Name is : #name")
```

Must result in:

```text
Name is : ahmad
```

without rescanning source syntax during normal runtime execution.

---

## Milestone C — Model and List

```cra
model User(
    id: Int
)

fun users(): List<User> {
    ...
}
```

Must resolve `List<User>` as a valid typed collection.

---

## Milestone D — Async Native Request

```cra
model User(
    id: Int
)

@AsyncAfter
fun getUsers(id: Int): List<User> {
    re CrossaRequest {
        path: "/v1/users",
        method: GET
    }
}
```

Must produce:

```text
Success(List<User>)
or
Failed(CrossaError)
```

through generated Android/iOS completion APIs.

---

## Milestone E — Parameterized Native Request

```cra
@AsyncAfter
fun getUser(id: Int): User {
    re CrossaRequest {
        path: "/v1/users/#id",
        method: GET
    }
}
```

Must compile the interpolation into the request plan rather than parse the path expression on every request.

---

# Roadmap Non-Negotiables

1. `.cra` is parsed only by the C++ frontend.
2. `#identifier` is the initial string interpolation syntax.
3. `List<T>` is a built-in collection type, not proof of general generics.
4. `CrossaRequest` is a native compiler/runtime builtin.
5. `CrossaRequest` is lowered to IR; it is not implemented by generated Kotlin/Swift networking code.
6. The containing function return type defines request response decoding.
7. `@AsyncAfter` return type is the logical success data type.
8. Generated `@AsyncAfter` APIs expose completion state.
9. Completion state has `Success(data)` and `Failed(error)` semantics.
10. Native networking, buffering, parsing, models, and scheduling remain in C++.
11. String interpolation is parsed/lowered ahead of runtime execution.
12. Async work uses the bounded shared Crossa scheduler.
13. Kotlin and Swift bindings remain thin.
14. Language growth follows real Crossa requirements.
15. Performance claims require complete-pipeline measurements.
