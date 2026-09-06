# ADR 0010: Native JSON Value Paths

## Status

Accepted.

## Context

ABI v1 value paths already navigate `NativeModel` fields and `NativeList`
elements. `RuntimeValue::Json` stores a Crossa-owned `JsonValue` DOM that is
not a model or list. `crossaGetPathKind` previously mapped Json to
`CrossaValueString`, and path segments could not enter object fields or array
elements. Android and iOS generators therefore rejected `Json` results rather
than exposing a native-backed view.

Platform JSON support must not copy native JSON into a string and parse it
again with `org.json`, Gson, Moshi, kotlinx.serialization, or Foundation
`JSONSerialization`.

## Decision

- ABI v1 remains the version. Additions are additive and do not change existing
  function signatures.
- `CrossaValueKind` gains `CrossaValueJson`. Json results report that category
  instead of `CrossaValueString`.
- The existing path segment kinds are reused. After a path reaches a Json
  `RuntimeValue`, remaining `CrossaAbiPathField` steps select object fields by
  source order and `CrossaAbiPathListElement` steps select array elements.
- New accessors expose Json kind, boolean, exact number text, string, size, and
  object keys. They do not expose STL, `JsonValue`, or native addresses.
- Kotlin `CrossaJson` is a borrowed view over `CrossaNativeResult` plus a
  compact path, matching nested model/list ownership.

## Alternatives

- Serializing Json to a platform string was rejected because it duplicates
  parsing and copies the native DOM.
- A second Android-only JSON object runtime was rejected because C++ owns JSON
  storage and navigation.
- Encoding object keys as path segments was rejected because the existing path
  ABI is index-based and keys remain native string views.

## Consequences

Callers that inspected `crossaGetPathKind` / `crossaGetResultKind` for an
explicit `Json` result previously observed `CrossaValueString`. They now
observe `CrossaValueJson`. No generated Android or iOS API consumed Json
results before this change. Nested model/list paths are unchanged.
