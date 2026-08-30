# ADR 0007: Android Native ABI Result Views

## Status

Accepted.

## Context

Android APIs must expose native `RuntimeValue`, `NativeList`, and `NativeModel`
results without eager Kotlin object graphs, reflection, string field lookup, or
raw C++ object pointers. JNI requires a narrow C-compatible boundary that
preserves result lifetime across asynchronous callback delivery.

## Decision

- The stable boundary is C ABI version 1 under `bindings/shared-abi`.
- A runtime context owns all result handles. Releasing the runtime invalidates
  every result it owns.
- A result handle owns the root `RuntimeValue`; immutable model/list storage
  remains alive through its existing controlled shared ownership.
- Model views are borrowed indexes within one root result. They are not
  independently allocated registry entries and cannot outlive the result.
- Generated field access uses schema declaration-order indexes. Kotlin never
  asks native code for a field by name.
- Strings cross the ABI as views valid only while their result handle remains
  retained. JNI copies a string only when Kotlin accesses that property.
- Kotlin exposes deterministic `AutoCloseable` ownership. Access after close
  fails before JNI is called.

## Consequences

The ABI remains independent of JNI and exposes no STL containers, C++ classes,
or native object addresses. Android views are lazy: list size, one element, and
one field each cross only when requested. Nested model/list fields require a
future ABI accessor with the same root-result ownership rule.

## Compatibility and Migration

This adds an internal ABI version axis without changing `.cra` syntax. Android
generated artifacts must require ABI version 1 before invoking result accessors.
