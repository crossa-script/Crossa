#include "crossa/compiler/semantic/SemanticScope.h"

#include <utility>

using namespace std;

namespace crossa::compiler::semantic {

    // Creates a value symbol with its resolved type and category.
    ValueSymbol::ValueSymbol(
        types::SemanticType type,
        ValueSymbolKind kind
    )
        : type_(std::move(type)), kind_(kind) {}

    // Returns the resolved value type.
    const types::SemanticType& ValueSymbol::getType() const noexcept {
        return type_;
    }

    // Returns the value ownership category.
    ValueSymbolKind ValueSymbol::getKind() const noexcept {
        return kind_;
    }

    // Creates a scope with an optional non-owning parent scope.
    SemanticScope::SemanticScope(const SemanticScope* parent) noexcept
        : parent_(parent) {}

    // Declares one value and returns false when the local name already exists.
    bool SemanticScope::declare(const string& name, ValueSymbol symbol) {
        return symbols_.emplace(name, std::move(symbol)).second;
    }

    // Resolves one value through this scope and its parent chain.
    const ValueSymbol* SemanticScope::resolve(const string& name) const noexcept {
        const auto iterator = symbols_.find(name);
        if (iterator != symbols_.end()) {
            return &iterator->second;
        }

        return parent_ == nullptr ? nullptr : parent_->resolve(name);
    }

}
