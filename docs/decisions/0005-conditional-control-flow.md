# ADR 0005: Conditional Control Flow in Crossa

## Status

Accepted

## Context

Crossa function bodies were previously linear. Supporting `if`, `else if`, and
`else` requires syntax nodes, typed branch scopes, platform-neutral branch IR,
and runtime execution without leaking branch-local values.

## Decision

Add block-based conditional statements with this syntax:

```cra
if (condition) {
    statements
} else if (anotherCondition) {
    statements
} else {
    statements
}
```

Conditions must be `Bool`. Each branch receives a child lexical scope. `else
if` is represented as a nested conditional in the else branch, preserving
source order and one-branch-only execution.

Add scalar comparison expressions for `==`, `!=`, `<`, `<=`, `>`, and `>=`.
Equality requires equal scalar types; ordering requires equal numeric types.

The IR owns structured conditional instructions. The interpreter executes the
selected block in a child runtime frame whose parent resolves captured values.

Value-returning functions require all paths through a conditional to return a
value; an `if` without an `else` does not satisfy that requirement.

## Consequences

- The canonical C++ frontend remains the only parser.
- Kotlin and Swift generators consume typed branch IR if they support source
  translation; they do not implement alternate language semantics.
- Branch-local values are unavailable after the branch at semantic and runtime
  levels.
- Loops, switch/when, and source-level error handling remain unsupported.
