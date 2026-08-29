# Crossa Language Foundation

> **Status:** Foundational language specification  
> **Language extension:** `.cra`  
> **Initial language version:** CRA Language V0  
> **Primary implementation language:** C++20  
> **Primary consumers:** Crossa compiler, native runtime/executor, Kotlin generator, Swift generator  
> **Related roadmap:** `docs/language/language-roadmap.md`  
> **Authority:** This document defines the supported foundational Crossa language surface. Syntax or semantics not defined here are not implicitly supported.

---

# 1. Definition

Crossa includes a deliberately small scripting and code-generation language stored in `.cra` files.

The language is not intended to become a general-purpose programming language.

It exists to provide a compact language for:

- executable Crossa functions,
- generated Android and iOS APIs,
- strongly typed models,
- project/runtime configuration,
- execution policy,
- native networking requests,
- and future Crossa runtime capabilities that are explicitly added to the language.

Crossa source is parsed and semantically validated once by the C++ compiler frontend.

The validated representation can then be:

1. executed by the Crossa C++ runtime,
2. translated into Kotlin,
3. translated into Swift,
4. or used to generate thin platform APIs over native runtime operations.

The language frontend and semantics remain platform-neutral.

---

# 2. Core Principle

The language exists to describe behavior.

The C++ compiler/runtime exists to perform Crossa's performance-critical work.

Kotlin and Swift exist to expose ergonomic APIs to their platforms.

These responsibilities must remain separate.

The canonical flow is:

```text
.cra source
    |
    v
C++ Source Loader
    |
    v
C++ Lexer
    |
    v
C++ Parser
    |
    v
AST
    |
    v
Semantic Analysis
    |
    v
Typed Crossa Representation
    |
    v
Crossa IR
    |
    +-----------------------------+
    |                             |
    v                             v
Native Runtime                Generators
                                  |
                           +------+------+
                           |             |
                           v             v
                        Kotlin          Swift
```

Kotlin and Swift must never implement independent `.cra` parsers.

---

# 3. Language Philosophy

Crossa language design follows these rules:

1. Keep syntax small.
2. Keep parsing deterministic.
3. Use explicit types.
4. Prefer compile-time knowledge.
5. Do not use runtime reflection in normal execution.
6. Keep target generators dependent on typed IR, not raw source.
7. Keep performance-critical runtime features native.
8. Keep one `.cra` source unit mapped to one primary generated platform identity.
9. Prefer declarative syntax for runtime configuration.
10. Describe scheduling policy without exposing physical threads.
11. Add syntax only for real Crossa requirements.
12. Avoid language complexity that belongs in C++ runtime internals.
13. Do not mimic Kotlin, Swift, JavaScript, Java, or C++ merely for familiarity.

Crossa should remain intentionally smaller than the languages it generates for.

---

# 4. Source Extension

Crossa source files use:

```text
.cra
```

Examples:

```text
UsersController.cra
User.cra
AuthController.cra
Math.cra
config.cra
```

The `.cra` extension is part of the language identity.

---

# 5. Source Unit Identity

An ordinary `.cra` file is one Crossa source unit.

For source files that generate a platform container, the filename stem defines the generated identity.

Example:

```text
UsersController.cra
```

maps conceptually to:

```text
Crossa source unit:
UsersController

Android:
UsersController.kt

iOS:
UsersController.swift
```

Top-level functions declared in that source unit become operations associated with that generated type.

Example:

```cra
fun add(a: Int, b: Int): Int {
    re a + b
}
```

inside:

```text
UsersController.cra
```

produces an `add` operation on the generated `UsersController` surface.

Filename-derived identity is deterministic build input.

Renaming the file can therefore be a generated API breaking change.

---

# 6. Model Files

Models should normally use one file per model.

Example:

```text
User.cra
```

```cra
model User(
    id: Int,
    name: String,
    isActivated: Bool
)
```

The primary model name should match the filename stem:

```text
User.cra -> User
```

Do not place unrelated models in one `.cra` file.

---

# 7. Reserved Declarative Files

Crossa may define explicitly reserved source filenames.

The initial reserved file is:

```text
config.cra
```

`config.cra` does not represent an ordinary generated class.

It defines project/runtime configuration.

Reserved filenames must always be documented.

The compiler must not invent hidden file-name behavior.

---

# 8. Initial Keywords and Builtins

Initial language declaration/control keywords:

```text
fun
re
var
model
config
```

Language builtin:

```text
print
CrossaRequest
```

Boolean literals:

```text
true
false
```

Execution annotations:

```text
@Sync
@Async
@AsyncAfter
```

Initial built-in type names:

```text
Int
String
Bool
List
```

Initial networking method literal:

```text
GET
```

Reserved words cannot be used as ordinary declaration identifiers.

---

# 9. Lexical Rules

## 9.1 Identifiers

Initial identifier grammar:

```text
[A-Za-z_][A-Za-z0-9_]*
```

Examples:

```text
user
userId
isActivated
UsersController
calculateTotal
_privateValue
```

Identifiers are case-sensitive.

---

## 9.2 Whitespace

Whitespace separates tokens where required.

Formatting does not change semantic behavior.

---

## 9.3 Strings

Strings use double quotes:

```cra
var name: String = "ahmad"
```

The initial source encoding is UTF-8.

---

## 9.4 Integers

Initial integer literals are base-10:

```cra
0
1
3000
-10
```

The exact native integer storage width must be explicitly standardized by the compiler/runtime and must not vary silently across platforms.

---

## 9.5 Booleans

Boolean literals are:

```cra
true
false
```

---

## 9.6 Comments

Initial comments use:

```cra
// comment
```

Block comments are not required by the foundation.

---

# 10. Type System

The initial type system is intentionally small.

---

## 10.1 `Int`

Example:

```cra
var id: Int = 10
```

Crossa `Int` is a platform-neutral language type.

It is not defined as Kotlin `Int`, Swift `Int`, or `std::int32_t` at the source-language level.

Its exact native storage must be deterministic once finalized.

---

## 10.2 `String`

Example:

```cra
var name: String = "ahmad"
```

Crossa owns the semantic string type.

Its internal native representation is a runtime/compiler implementation concern.

---

## 10.3 `Bool`

Example:

```cra
var isActivated: Bool = true
```

Values are:

```text
true
false
```

---

## 10.4 Model Types

A model declaration creates a named language type.

Example:

```cra
model User(
    id: Int
)
```

makes:

```text
User
```

a valid type reference.

---

## 10.5 `List<T>`

Crossa includes a built-in typed list.

Examples:

```cra
List<Int>
List<String>
List<User>
```

Example function:

```cra
fun getUsers(): List<User> {
    ...
}
```

`List<T>` is a built-in parameterized language type.

This does **not** mean Crossa supports general user-defined generics.

For V0:

- `List<T>` accepts exactly one element type.
- `T` must resolve to a valid Crossa type.
- The element type is preserved in semantic analysis and IR.
- Native/runtime-backed lists should avoid eager platform object duplication where possible.

---

# 11. Variables

Variables use explicit typed syntax:

```cra
var <name>: <Type> = <expression>
```

Examples:

```cra
var name: String = "ahmad"
var age: Int = 20
var enabled: Bool = true
```

Type inference is not part of V0.

Invalid until explicitly introduced:

```cra
var name = "ahmad"
```

Explicit source types keep diagnostics and generation deterministic.

---

# 12. String Interpolation

Crossa strings support interpolation using:

```text
#<identifier>
```

Example:

```cra
var name: String = "ahmad"
print("Name is : #name")
```

Result:

```text
Name is : ahmad
```

The `#` character inside a string begins interpolation only when it is immediately followed by a valid identifier.

Examples:

```cra
print("Hello #name")
print("User ID: #userId")
print("Count: #count")
```

---

## 12.1 Interpolation Scope

The interpolated identifier must resolve in the current semantic scope.

Valid sources include:

- function parameters,
- visible source/function variables.

Example:

```cra
fun greet(name: String) {
    print("Hello #name")
}
```

Unknown identifiers are compile errors.

Example:

```cra
print("Hello #unknown")
```

must not silently print an empty value.

---

## 12.2 Initial Interpolation Limit

V0 supports simple identifiers:

```text
#name
#id
#userId
```

V0 does not automatically support:

```text
#{name}
#user.name
#users[0]
#getName()
```

Those forms require explicit future language design.

---

## 12.3 Interpolation Compilation

The runtime must not repeatedly scan raw source strings for `#` during normal execution.

The lexer/parser/compiler should represent:

```cra
"Name is : #name"
```

conceptually as:

```text
InterpolatedString
    LiteralSegment("Name is : ")
    IdentifierSegment(name)
```

The compiler lowers this into a string construction plan.

For networking paths, the same system can lower parameter interpolation directly into native request encoding metadata.

This is important for performance.

---

# 13. Functions

Functions use:

```text
fun
```

Example:

```cra
fun add(a: Int, b: Int): Int {
    re a + b
}
```

General form:

```cra
fun functionName(parameterName: Type, ...): ReturnType {
    statements
}
```

---

## 13.1 Parameters

Parameters use:

```cra
name: Type
```

Example:

```cra
fun greet(name: String): String {
    re name
}
```

---

## 13.2 Return Type

A value-returning function declares its logical result type:

```cra
fun add(a: Int, b: Int): Int {
    re a + b
}
```

A function without a value may omit the return type:

```cra
fun showName(name: String) {
    print(name)
}
```

The compiler can internally use a unit/void semantic type without exposing a `Void` keyword in source.

---

# 14. `re`

Crossa uses:

```text
re
```

as the return keyword.

Example:

```cra
re a + b
```

For a normal synchronous function, `re` returns the value.

For an `@AsyncAfter` function, `re` defines the logical success value or the native operation whose eventual success value matches the declared logical result type.

---

## 14.1 Return Validation

Semantic analysis must reject:

- incompatible return value types,
- missing required return values,
- value returns from no-value functions,
- invalid request result types.

---

# 15. Function Calls

Normal call syntax:

```cra
add(1, 2)
```

Example:

```cra
fun add(a: Int, b: Int): Int {
    re a + b
}

var result: Int = add(1, 2)
```

The compiler validates:

- function existence,
- argument count,
- argument type,
- return type usage.

Function overloading is not part of V0.

A source/project scope should not declare ambiguous functions with the same callable identity.

At the top level, a function call is an executable entry point. Function
declarations are not invoked merely because they exist; only calls written in
the source execute during direct script execution.

---

# 16. Expressions

Initial expressions include:

- literals,
- identifiers,
- function calls,
- parenthesized expressions,
- basic arithmetic,
- interpolated strings,
- `CrossaRequest`.

Initial arithmetic operators:

```text
+
-
*
/
```

Precedence:

```text
1. Parentheses
2. * /
3. + -
```

The parser builds the expression tree before code generation.

Top-level executable expressions are restricted to function calls. Their
results are discarded unless another call, such as `print`, consumes them.

---

# 17. `print`

Crossa provides:

```cra
print(...)
```

as a builtin.

Example:

```cra
print("Hello")
```

Language semantics:

> Render the value followed by a line terminator.

With interpolation:

```cra
var name: String = "ahmad"
print("Name is : #name")
```

prints:

```text
Name is : ahmad
```

Conceptual target behavior:

Android:

```kotlin
println(value)
```

Swift:

```swift
print(value)
```

Native execution routes output through a focused Crossa runtime output/logging abstraction.

Do not scatter direct standard-output calls throughout compiler/runtime code.

---

# 18. Models

Models define typed data.

Example:

```cra
model User(
    id: Int,
    name: String,
    isActivated: Bool
)
```

Fields use:

```cra
name: Type
```

Field order is semantically stable unless a later specification says otherwise.

---

## 18.1 Minimal Model

Valid:

```cra
model User(
    id: Int
)
```

This model can then be used as:

```cra
User
List<User>
```

---

## 18.2 Android Representation

For materialized pure generated code, a model may conceptually generate:

```kotlin
data class User(
    val id: Int
)
```

For native-backed high-performance results, Android may instead expose a native-backed model/view.

The representation strategy must preserve Crossa model semantics.

---

## 18.3 iOS Representation

A materialized Swift representation may conceptually be:

```swift
struct User {
    let id: Int
}
```

Native-backed results may instead use a safe wrapper/view.

Again, platform representation does not define the language semantics.

---

# 19. Execution Annotations

Functions may have one execution-policy annotation.

Supported:

```text
@Sync
@Async
@AsyncAfter
```

Only one execution annotation may apply to a function.

These annotations define scheduling/completion semantics.

They do not expose raw operating-system thread creation.

---

# 20. `@Sync`

Example:

```cra
@Sync
fun add(a: Int, b: Int): Int {
    re a + b
}
```

Behavior:

1. Execute in the current Crossa execution context.
2. Complete before the caller's invocation is considered finished.
3. Return the result or error through the synchronous API contract.

No annotation means synchronous behavior in V0.

Therefore:

```cra
fun add(a: Int, b: Int): Int {
    re a + b
}
```

is synchronous by default.

---

# 21. `@Async`

Example:

```cra
@Async
fun refresh() {
    print("refresh")
}
```

`@Async` schedules work through the shared Crossa scheduler.

It does **not** mean:

```text
new std::thread per call
```

Crossa must use bounded runtime scheduling.

V0 `@Async` is intended for fire-and-forget behavior.

A meaningful completion result should use `@AsyncAfter`.

---

# 22. `@AsyncAfter`

`@AsyncAfter` executes the function asynchronously and sends a terminal state back to the caller.

Example:

```cra
@AsyncAfter
fun calculate(a: Int, b: Int): Int {
    re a + b
}
```

The declared return type:

```text
Int
```

is the logical success data type.

The generated platform function does not expose a synchronous `Int` return.

Instead, it exposes a lambda/closure receiving a state.

---

# 23. Async Completion State

The semantic completion contract is:

```text
CrossaState<T>
    |
    +-- Success<T>
    |      data: T
    |
    +-- Failed
           error: CrossaError
```

`T` is the `.cra` function's declared logical result type.

Example:

```cra
@AsyncAfter
fun getUsers(id: Int): List<User> {
    ...
}
```

means the completion state contains:

```text
Success(List<User>)
```

or:

```text
Failed(CrossaError)
```

---

## 23.1 Android Concept

Generated Android API concept:

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

Function concept:

```kotlin
fun getUsers(
    id: Int,
    onState: (CrossaState<List<User>>) -> Unit
)
```

`Success` and `Failed` are generated state data representations.

The final exact package/type naming belongs to Android generator documentation.

---

## 23.2 Swift Concept

Swift exposes the same semantic states using an idiomatic Swift type.

Concept:

```swift
enum CrossaState<T> {
    case success(T)
    case failed(CrossaError)
}
```

Function concept:

```swift
func getUsers(
    id: Int,
    onState: @escaping (CrossaState<[User]>) -> Void
)
```

The language semantics are:

```text
Success(data)
Failed(error)
```

regardless of the target's physical representation.

---

## 23.3 Completion Guarantees

For `@AsyncAfter`:

- the operation executes through the Crossa scheduler,
- success/failure state originates from native execution,
- completion is delivered exactly once,
- native errors are not swallowed,
- the platform bridge does not re-execute the function,
- state ownership remains valid through delivery,
- shutdown/cancellation must be handled safely once cancellation semantics exist.

---

# 24. Execution Annotation Validation

Invalid:

```cra
@Sync
@Async
fun load() {
}
```

Invalid:

```cra
@Async
@AsyncAfter
fun load() {
}
```

Allowed policies:

```text
none
@Sync
@Async
@AsyncAfter
```

Validation belongs to semantic analysis.

---

# 25. `config.cra`

Initial networking/runtime configuration:

```cra
config {
    baseUrl: "https://api.example.com",
    timeoutRequest: 3000,
    interceptor: true
}
```

Recognized initial keys:

```text
baseUrl: String
timeoutRequest: Int
interceptor: Bool
```

`timeoutRequest` unit:

```text
milliseconds
```

Unknown keys are compile errors.

Duplicate keys are compile errors.

`config.cra` is declarative and should not contain arbitrary functions in V0.

---

# 26. `CrossaRequest`

`CrossaRequest` is a built-in native networking expression.

Example:

```cra
re CrossaRequest {
    path: "/v1/users",
    method: GET
}
```

It is not:

- a user-defined model,
- a normal function,
- a generated Kotlin HTTP client,
- a generated Swift HTTP client.

It represents a typed native network operation understood by the compiler and Crossa Network runtime.

---

# 27. CrossaRequest Example

Complete example:

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

The source function:

```text
getUsers
```

accepts:

```text
id: Int
```

and has logical success type:

```text
List<User>
```

Because it is `@AsyncAfter`, generated callers receive:

```text
Success(List<User>)
or
Failed(CrossaError)
```

through their target completion mechanism.

---

# 28. CrossaRequest Response Type

The containing function return type defines the expected successful request result.

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

The compiler knows:

```text
Expected response type = List<User>
```

The intended runtime pipeline is:

```text
Request invocation
    ->
native request plan
    ->
C++ request encoding
    ->
C++ transport
    ->
native response buffer
    ->
generated/schema-aware C++ decoding
    ->
native List<User>
    ->
CrossaState<List<User>>
    ->
Android lambda / Swift closure
```

Kotlin and Swift must not parse the network response.

---

# 29. CrossaRequest Initial Fields

V0 request syntax requires:

```text
path
method
```

Example:

```cra
CrossaRequest {
    path: "/v1/users",
    method: GET
}
```

Initial guaranteed method literal:

```text
GET
```

Additional request properties and methods must be added explicitly in networking/language documentation.

Potential future properties include:

```text
query
headers
body
timeout
auth
retry
multipart
download
```

Potential future methods include:

```text
POST
PUT
PATCH
DELETE
HEAD
```

They are not implicitly supported by this foundation until specified and implemented.

---

# 30. Request Path Interpolation

Crossa string interpolation also applies to request paths.

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

The compiler resolves:

```text
#id
```

to the function parameter:

```text
id: Int
```

The runtime must not scan raw path source looking for `#id` on every request.

The compiler should lower the path to a request plan such as:

```text
StaticSegment("/v1/users/")
ParameterSegment(id)
```

The native request encoder then serializes the parameter efficiently.

---

# 31. Native Networking Rule

Networking remains C++ owned.

For `CrossaRequest`, C++ owns:

- path construction,
- interpolation application,
- request plan,
- query/header/body encoding when supported,
- transport,
- connection reuse,
- timeout state,
- cancellation state,
- response buffering,
- response parsing,
- model/list creation,
- errors,
- async state,
- scheduler integration.

Generated Kotlin/Swift code is a thin calling/completion surface.

---

# 32. `List<User>` Response Ownership

A request returning:

```cra
List<User>
```

does not imply that C++ must eagerly build a duplicate JVM/Swift object for every user before the result can be delivered.

For large results, Crossa should preserve the architecture's native-backed model/list strategy where practical.

The source-language type is:

```text
List<User>
```

The platform physical representation may be:

- materialized target collection for small/pure generated cases,
- native-backed list/view for high-performance runtime-backed cases.

This decision must preserve type semantics and explicit ownership.

---

# 33. Pure Translation vs Native Runtime

Crossa supports two backend strategies.

## Pure Source Translation

Simple pure script logic can be translated when that build strategy is intentionally selected.

Example:

```cra
fun add(a: Int, b: Int): Int {
    re a + b
}
```

can conceptually generate equivalent Kotlin/Swift logic.

## Native Runtime Binding

Runtime-backed operations remain native.

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

must not generate a separate Retrofit/URLSession implementation.

Instead:

```text
Generated Kotlin / Swift API
    ->
Crossa native ABI
    ->
Crossa C++ runtime
```

The backend strategy must be explicit and deterministic.

---

# 34. AST Foundation

Initial AST families should cover:

```text
SourceFile
FunctionDeclaration
ParameterDeclaration
VariableDeclaration
ModelDeclaration
ModelFieldDeclaration
ConfigDeclaration
ConfigEntry
ReturnStatement
ExpressionStatement
CallExpression
BinaryExpression
IdentifierExpression
StringLiteral
InterpolatedStringExpression
IntegerLiteral
BooleanLiteral
ListTypeReference
NamedTypeReference
Annotation
CrossaRequestExpression
RequestEntry
HttpMethodLiteral
```

AST represents syntax only.

---

# 35. Semantic Analysis

Semantic analysis owns:

- symbol registration,
- duplicate detection,
- scope resolution,
- type resolution,
- parameter validation,
- variable validation,
- function calls,
- returns,
- models,
- `List<T>`,
- interpolation identifier resolution,
- execution annotations,
- config validation,
- `CrossaRequest` validation,
- request return-type validation.

Generators must not perform fundamental language validation.

---

# 36. IR Integration

The frontend layering is:

```text
.cra
    ->
AST
    ->
typed semantic model
    ->
Crossa IR
    ->
execution / generators
```

For a request:

```cra
@AsyncAfter
fun getUsers(id: Int): List<User> {
    re CrossaRequest {
        path: "/v1/users",
        method: GET
    }
}
```

IR should conceptually know:

```text
Function:
    name = getUsers

Parameter:
    id: Int

ExecutionPolicy:
    AsyncAfter

LogicalResultType:
    List<User>

Return:
    NativeRequest

NativeRequest:
    method = GET
    pathPlan = Static("/v1/users")
    responseType = List<User>
```

IR must not encode Kotlin/Swift-specific callback classes.

---

# 37. Interpolated String IR

Example:

```cra
"Name is : #name"
```

can lower conceptually to:

```text
StringBuildPlan
    Static("Name is : ")
    ReadSymbol(name)
```

Example request:

```cra
"/v1/users/#id"
```

can lower to:

```text
RequestPathPlan
    Static("/v1/users/")
    Parameter(id)
```

This avoids repeated source parsing at runtime.

---

# 38. Initial Grammar

Foundation grammar:

```ebnf
source_file            = { declaration } ;

declaration            = annotated_function
                       | function_declaration
                       | variable_declaration
                       | model_declaration
                       | config_declaration
                       | top_level_expression_statement ;

top_level_expression_statement
                       = function_call ;

annotated_function     = execution_annotation,
                         function_declaration ;

execution_annotation   = "@Sync"
                       | "@Async"
                       | "@AsyncAfter" ;

function_declaration   = "fun", identifier,
                         "(", [ parameter_list ], ")",
                         [ ":", type_reference ],
                         block ;

parameter_list         = parameter, { ",", parameter } ;

parameter              = identifier, ":", type_reference ;

variable_declaration   = "var", identifier, ":",
                         type_reference, "=", expression ;

model_declaration      = "model", identifier,
                         "(", [ model_field_list ], ")" ;

model_field_list       = model_field, { ",", model_field } ;

model_field            = identifier, ":", type_reference ;

config_declaration     = "config", "{",
                         [ config_entry_list ], "}" ;

config_entry_list      = config_entry, { ",", config_entry } ;

config_entry           = identifier, ":", expression ;

block                  = "{", { statement }, "}" ;

statement              = return_statement
                       | variable_declaration
                       | expression_statement ;

return_statement       = "re", expression ;

expression_statement   = expression ;

expression             = additive_expression ;

additive_expression    = multiplicative_expression,
                         { ("+" | "-"),
                           multiplicative_expression } ;

multiplicative_expression
                       = primary_expression,
                         { ("*" | "/"),
                           primary_expression } ;

primary_expression     = literal
                       | identifier
                       | function_call
                       | crossa_request_expression
                       | "(", expression, ")" ;

function_call          = identifier,
                         "(", [ argument_list ], ")" ;

argument_list          = expression, { ",", expression } ;

crossa_request_expression
                       = "CrossaRequest", "{",
                         request_entry_list, "}" ;

request_entry_list     = request_entry,
                         { ",", request_entry } ;

request_entry          = "path", ":", string_literal
                       | "method", ":", http_method ;

http_method            = "GET" ;

literal                = string_literal
                       | integer_literal
                       | boolean_literal ;

type_reference         = scalar_type
                       | named_type
                       | list_type ;

scalar_type            = "Int"
                       | "String"
                       | "Bool" ;

list_type              = "List", "<",
                         type_reference, ">" ;

named_type             = identifier ;
```

String interpolation is parsed inside `string_literal` content according to the interpolation rules.

---

# 39. Parser Ownership

`.cra` parsing is implemented once in C++.

Never create:

```text
Kotlin CRA parser
Swift CRA parser
Android CRA runtime parser
iOS CRA runtime parser
```

The platform generators consume validated semantic/IR data.

---

# 40. Parser Performance

Prefer:

- direct source-buffer access,
- compact tokens,
- source ranges/views,
- explicit parser state,
- bounded recursion,
- deterministic error recovery,
- minimal token text duplication.

Avoid one heap allocation per token where practical.

Do not micro-optimize before correctness and measurements.

---

# 41. Token Model

A token conceptually contains:

```text
kind
source range
literal metadata when necessary
```

Initial token kinds may include:

```text
Identifier
IntegerLiteral
StringLiteral
BooleanLiteral

KeywordFun
KeywordRe
KeywordVar
KeywordModel
KeywordConfig
KeywordPrint
KeywordList
KeywordCrossaRequest

AnnotationSync
AnnotationAsync
AnnotationAsyncAfter

MethodGet

LeftParen
RightParen
LeftBrace
RightBrace
LeftAngle
RightAngle
Colon
Comma
Equal
Plus
Minus
Star
Slash
Hash
EndOfFile
```

Exact implementation enum organization follows Crossa C++ file/type rules.

---

# 42. Diagnostics

Language diagnostics should provide:

```text
file
line
column
diagnostic code
short message
relevant symbol
```

Example:

```text
UsersController.cra:4:20 CRA2004
Unknown interpolation identifier 'name'.
```

Example:

```text
UsersController.cra:7:8 CRA6002
@AsyncAfter function expects logical result 'List<User>'.
```

Example:

```text
UsersController.cra:8:17 CRA7004
Unknown CrossaRequest method 'FETCH'.
```

Do not expose raw parser/runtime exceptions.

---

# 43. Diagnostic Domains

A future stable diagnostic layout can use:

```text
CRA1xxx  source / lexer / parser
CRA2xxx  symbol / semantic analysis
CRA3xxx  type system
CRA4xxx  models
CRA5xxx  config
CRA6xxx  execution policy
CRA7xxx  native request / IR lowering
CRA8xxx  generators
CRA9xxx  internal compiler invariant
```

Exact codes can be finalized during implementation.

---

# 44. Deterministic Generation

Identical:

- `.cra` sources,
- compiler version,
- target,
- configuration,
- runtime ABI,
- dependency/toolchain set,

must produce deterministic output.

Generated symbol names and declaration order must be stable.

Generated source should not contain volatile timestamps unless they are explicitly isolated to non-reproducible metadata.

---

# 45. Generated Source Ownership

Generated Kotlin/Swift files are compiler output.

Users should not manually maintain them as source of truth.

The source of truth is `.cra` plus Crossa configuration and compiler/runtime implementation.

Generated files may include a generated-code marker.

---

# 46. Naming

Initial convention:

```text
Models / generated source types: PascalCase
Functions: camelCase
Variables: camelCase
Parameters: camelCase
Config keys: camelCase
```

Examples:

```cra
model UserProfile(
    userId: Int,
    displayName: String
)

fun calculateTotal(a: Int, b: Int): Int {
    re a + b
}
```

---

# 47. File Naming

Ordinary source-unit files:

```text
PascalCase.cra
```

Examples:

```text
UsersController.cra
User.cra
AuthService.cra
```

Reserved files:

```text
config.cra
```

Avoid case-only identity collisions because target filesystems differ in case sensitivity.

---

# 48. Android Mapping

Typical logical mappings:

```text
Crossa Int          -> Kotlin Int or defined native-backed representation
Crossa String       -> Kotlin String or native-backed representation
Crossa Bool         -> Kotlin Boolean
Crossa List<T>      -> List<T> or native-backed list view
Crossa model        -> data class or native-backed model
@Sync               -> synchronous API
@Async              -> asynchronous/fire-and-forget API
@AsyncAfter         -> completion lambda receiving CrossaState<T>
print               -> println semantics
CrossaRequest       -> native runtime call
```

`CrossaRequest` must not translate into a separate Kotlin networking engine.

---

# 49. Swift Mapping

Typical logical mappings:

```text
Crossa Int          -> Swift Int or defined native-backed representation
Crossa String       -> Swift String or native-backed representation
Crossa Bool         -> Swift Bool
Crossa List<T>      -> [T] or native-backed collection view
Crossa model        -> struct/class or native-backed model
@Sync               -> synchronous API
@Async              -> async/fire-and-forget API
@AsyncAfter         -> completion closure receiving CrossaState<T>
print               -> Swift print semantics
CrossaRequest       -> native runtime call
```

`CrossaRequest` must not translate into a separate URLSession request implementation.

---

# 50. Scheduler Semantics

Execution annotations describe execution policy, not physical thread creation.

Rules:

1. Use the shared Crossa scheduler.
2. Do not create one OS thread per async call.
3. Worker resources must remain bounded.
4. Heavy work must not run on platform UI/main threads.
5. Native async state has explicit ownership.
6. Completion delivery uses explicit platform bridges.
7. Platform bindings do not implement a second Crossa scheduler.

---

# 51. Networking Execution

For:

```cra
@AsyncAfter
fun getUsers(id: Int): List<User> {
    re CrossaRequest {
        path: "/v1/users",
        method: GET
    }
}
```

the intended native flow is:

```text
Generated Android/iOS invocation
        |
        v
Stable Crossa boundary
        |
        v
Native operation state
        |
        v
Shared scheduler
        |
        v
Native request encoder
        |
        v
Native transport
        |
        v
Native response buffer
        |
        v
Generated/schema-specific C++ decoder
        |
        v
Native List<User>
        |
        v
Success(data) / Failed(error)
        |
        v
Thin platform completion bridge
```

This is a core Crossa architectural invariant.

---

# 52. Native Response Parsing

The logical function return type drives native response decoding.

Examples:

```text
User
List<User>
String
Bool
Int
```

When a request declares:

```cra
fun getUsers(): List<User>
```

the native decoder knows the expected logical response schema from typed IR.

Do not make the platform generator rediscover the response schema.

---

# 53. Error Semantics

Source-level exception syntax is not part of V0.

Native operations still use structured Crossa errors.

`@AsyncAfter` exposes terminal failure as:

```text
Failed(CrossaError)
```

The error should preserve:

- stable domain,
- stable code,
- message,
- retryability/recoverability when known,
- optional metadata.

Errors must not disappear at the platform boundary.

---

# 54. Unsupported V0 Features

The initial foundation does not automatically include:

```text
if / else
for
while
switch / when
try / catch
nullable syntax
enums
maps
user-defined generic types
imports
packages
visibility modifiers
inheritance
interfaces
reflection
eval
closures inside .cra
operator overloading
extension functions
```

They require explicit language design.

---

# 55. No Hidden Dynamic Behavior

Avoid:

- runtime function lookup by arbitrary strings,
- monkey patching,
- runtime class modification,
- reflection-driven field access in hot paths,
- eval,
- execution of arbitrary generated source text.

Crossa language behavior should be statically analyzable by the compiler wherever practical.

---

# 56. Cross-File Resolution

The compiler can load a known project source set and build a symbol table across `.cra` files.

Exact import/visibility syntax is not part of V0.

Until imports are designed, project-level source discovery/build configuration defines the compilation source set.

---

# 57. Direct Script Execution

Crossa should eventually support executing a valid `.cra` source through CLI/native tooling.

Conceptual command:

```text
crossa run UsersController.cra
```

The exact CLI command is not fixed here.

Execution must still pass through:

```text
parse
validate
lower
execute
```

Declarations are compiled but remain inert during direct execution. Top-level
function calls execute in source order after validation and lowering. A call's
result is discarded unless it is passed to `print`.

No unsafe "execute unvalidated source text" shortcut is allowed.

---

# 58. Project Compilation

A project may contain:

```text
config.cra
User.cra
UsersController.cra
AuthController.cra
```

The compiler should:

1. load the source set,
2. tokenize/parse,
3. register symbols,
4. perform semantic analysis,
5. lower typed IR,
6. link required runtime modules,
7. generate/execute requested targets.

---

# 59. Security

`.cra` source is compiler input and must be treated as untrusted.

Guard against:

- pathological nesting,
- integer overflow during literal parsing,
- invalid UTF-8,
- huge source/token counts,
- oversized literals,
- parser recursion exhaustion,
- malformed interpolations,
- malformed annotations,
- duplicate symbols,
- invalid model types,
- invalid request fields,
- invalid list element types.

Malformed source must never cause undefined behavior.

---

# 60. Compiler Organization

Recommended conceptual source boundaries:

```text
compiler/
    source/
    lexer/
    parser/
    ast/
    semantic/
    types/
    diagnostics/
    ir/
    generators/
```

Generators:

```text
compiler/generators/
    native/
    kotlin/
    swift/
```

Do not create empty abstraction layers just to mirror this layout.

Directories should exist when real implementation requires them.

---

# 61. Generator Responsibility

Generators consume typed semantic/IR input.

They own:

- target syntax,
- target naming,
- target formatting,
- target wrapper shapes.

They do not own:

- `.cra` parsing,
- symbol resolution,
- type checking,
- interpolation resolution,
- `CrossaRequest` validation,
- scheduling semantics,
- native response parsing semantics.

---

# 62. Language Versioning

Language version, compiler version, IR version, and runtime ABI version are separate concepts.

Initial internal language label:

```text
CRA Language V0
```

Once external projects depend on `.cra` syntax, compatibility must be treated deliberately.

---

# 63. Language Evolution

Changes requiring compatibility review include:

- new reserved keywords,
- changed interpolation syntax,
- changed `List<T>` semantics,
- changed operator precedence,
- changed integer representation semantics,
- changed source-unit identity,
- changed `@AsyncAfter` state semantics,
- changed model semantics,
- changed `CrossaRequest` field meaning,
- changed config key behavior,
- changed generated public API meaning.

Unsupported syntax must fail clearly rather than be silently reinterpreted.

---

# 64. Language Documentation

This file is the root language specification:

```text
docs/language/language-foundation.md
```

The implementation roadmap is:

```text
docs/language/language-roadmap.md
```

Future focused documents may include:

```text
docs/language/functions.md
docs/language/models.md
docs/language/string-interpolation.md
docs/language/async-execution.md
docs/language/configuration.md
docs/language/networking.md
docs/language/type-system.md
docs/language/grammar.md
```

Do not duplicate the entire foundation into those documents.

---

# 65. First End-to-End Language Milestone

Input:

```cra
fun add(a: Int, b: Int): Int {
    re a + b
}

print(add(1, 2))
```

Crossa must be able to:

1. read the source,
2. tokenize in C++,
3. parse in C++,
4. build AST,
5. resolve types,
6. lower to IR,
7. execute `print(add(1, 2))` natively and print `3`,
8. generate deterministic Kotlin,
9. generate deterministic Swift.

---

# 66. String Interpolation Milestone

Input:

```cra
var name: String = "ahmad"
print("Name is : #name")
```

Crossa must semantically represent interpolation and produce:

```text
Name is : ahmad
```

without re-parsing raw `.cra` string syntax every execution.

---

# 67. Model/List Milestone

Input:

```cra
model User(
    id: Int
)
```

Crossa must register:

```text
User
List<User>
```

as valid typed semantic representations.

---

# 68. Native Request Milestone

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

Crossa must understand:

```text
Function:
getUsers

Parameter:
id: Int

Logical success type:
List<User>

Execution:
AsyncAfter

Operation:
Native CrossaRequest

HTTP method:
GET

Completion:
Success(List<User>) | Failed(CrossaError)
```

The native runtime owns request execution and parsing.

---

# 69. Request Interpolation Milestone

Input:

```cra
@AsyncAfter
fun getUser(id: Int): User {
    re CrossaRequest {
        path: "/v1/users/#id",
        method: GET
    }
}
```

The compiler resolves `#id` to the typed function parameter and emits a native request path plan.

No runtime `.cra` source scanning is required for normal request execution.

---

# 70. Architectural Non-Negotiables

1. `.cra` is the Crossa language extension.
2. `.cra` has one canonical C++ frontend.
3. Kotlin and Swift do not parse `.cra`.
4. AST represents syntax.
5. Semantic analysis owns validation and meaning.
6. IR is platform-neutral.
7. `fun` declares functions.
8. `re` returns logical function results.
9. `var` declares explicitly typed variables.
10. `print` has line-print semantics.
11. `#identifier` is the initial string interpolation syntax.
12. Interpolation is parsed/lowered before runtime hot-path execution.
13. `model` declares typed models.
14. `List<T>` is a built-in typed list.
15. `List<T>` does not imply general user-defined generics.
16. `config.cra` is the initial reserved declarative config file.
17. `@Sync`, `@Async`, and `@AsyncAfter` are execution policies.
18. Async execution uses the shared bounded runtime scheduler.
19. `@AsyncAfter` delivers `Success(data)` or `Failed(error)` semantics.
20. The source return type of `@AsyncAfter` is the logical success data type.
21. `CrossaRequest` is a language/compiler/runtime builtin.
22. `CrossaRequest` lowers to native IR rather than target networking code.
23. The containing function return type defines native response decoding.
24. Networking execution, response buffering, parsing, and native result ownership remain C++ responsibilities.
25. Android and iOS bindings remain thin.
26. Generated output is deterministic.
27. One ordinary `.cra` source file maps to one primary generated platform identity.
28. Models normally follow one-file-per-type.
29. Unsupported syntax fails with diagnostics.
30. The language grows only for concrete Crossa requirements.

---

# 71. Current Open Decisions

Do not invent these during unrelated tasks:

- exact native `Int` storage width,
- exact native string representation,
- exact native `List<T>` binary layout,
- exact Kotlin package naming,
- exact Swift module naming,
- exact public name of the generated state wrapper if `CrossaState` changes,
- exact `CrossaError` public target representation,
- whether pure functions default to translation or native wrapping in production,
- cancellation syntax,
- nullable syntax,
- enum syntax,
- control-flow syntax,
- imports/packages,
- visibility,
- model mutability,
- request headers/query/body syntax,
- additional HTTP methods,
- interceptor API,
- source-level error handling,
- exact CLI commands.

Those decisions require focused design.

---

# 72. Final Directive

Crossa language must remain simple to read:

```cra
var name: String = "ahmad"
print("Name is : #name")
```

should obviously mean:

```text
Create a typed String variable.
Print a string containing its value.
```

This:

```cra
model User(
    id: Int
)
```

should obviously define a typed model.

And this:

```cra
@AsyncAfter
fun getUsers(id: Int): List<User> {
    re CrossaRequest {
        path: "/v1/users",
        method: GET
    }
}
```

should mean:

```text
Expose getUsers.
Accept id: Int.
Execute the operation asynchronously through Crossa.
Perform the request in the native C++ runtime.
Parse the successful response natively as List<User>.
Deliver Success(data) or Failed(error) to the caller.
```

The language describes the work.

The C++ runtime performs the performance-critical work.

The generators expose the result naturally to Android and iOS.
