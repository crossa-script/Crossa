#pragma once

#include <string>
#include <unordered_map>

#include "crossa/compiler/semantic/TypedExpression.h"
#include "crossa/compiler/types/SemanticType.h"

namespace crossa::compiler::semantic {

// Describes one resolved value name and its owning scope category.
class ValueSymbol final {
public:
    // Creates a value symbol with its resolved type and category.
    ValueSymbol(types::SemanticType type, ValueSymbolKind kind);

    // Returns the resolved value type.
    [[nodiscard]] const types::SemanticType& getType() const noexcept;

    // Returns the value ownership category.
    [[nodiscard]] ValueSymbolKind getKind() const noexcept;

private:
    types::SemanticType type_;
    ValueSymbolKind kind_;
};

// Stores values visible in one lexical scope and resolves through its parent.
// declare() detects local duplicates while resolve() supports lexical lookup.
class SemanticScope final {
public:
    // Creates a scope with an optional non-owning parent scope.
    explicit SemanticScope(const SemanticScope* parent = nullptr) noexcept;

    // Declares one value and returns false when the local name already exists.
    [[nodiscard]] bool declare(
        const std::string& name,
        ValueSymbol symbol
    );

    // Resolves one value through this scope and its parent chain.
    [[nodiscard]] const ValueSymbol* resolve(
        const std::string& name
    ) const noexcept;

private:
    const SemanticScope* parent_;
    std::unordered_map<std::string, ValueSymbol> symbols_;
};

}
