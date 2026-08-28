#include "crossa/compiler/ast/TypeReference.h"

#include <utility>

using namespace std;

namespace crossa::compiler::ast {

    // Creates a named scalar or model type reference.
    TypeReference TypeReference::createNamed(
        string name,
        source::SourceLocation location
    ) {
        return TypeReference(
            TypeReferenceKind::Named,
            std::move(name),
            nullptr,
            location
        );
    }

    // Creates a List type containing one element type.
    TypeReference TypeReference::createList(
        TypeReference elementType,
        source::SourceLocation location
    ) {
        return TypeReference(
            TypeReferenceKind::List,
            "List",
            make_unique<TypeReference>(std::move(elementType)),
            location
        );
    }

    // Returns whether this reference is named or a List.
    TypeReferenceKind TypeReference::getKind() const noexcept {
        return kind_;
    }

    // Returns the scalar or model name for a named type.
    const string& TypeReference::getName() const noexcept {
        return name_;
    }

    // Returns the List element type or null for a named type.
    const TypeReference* TypeReference::getElementType() const noexcept {
        return elementType_.get();
    }

    // Returns the location where this type reference begins.
    const source::SourceLocation& TypeReference::getLocation() const noexcept {
        return location_;
    }

    // Creates the internal representation for one type shape.
    TypeReference::TypeReference(
        TypeReferenceKind kind,
        string name,
        unique_ptr<TypeReference> elementType,
        source::SourceLocation location
    )
        : kind_(kind),
          name_(std::move(name)),
          elementType_(std::move(elementType)),
          location_(location) {}

}
