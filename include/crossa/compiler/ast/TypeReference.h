#pragma once

#include <memory>
#include <string>

#include "crossa/compiler/source/SourceLocation.h"

namespace crossa::compiler::ast {

// Distinguishes named Crossa types from the built-in List type.
enum class TypeReferenceKind {
    Named,
    List
};

// Represents a scalar, model, or List<T> type written in Crossa source.
// createNamed() and createList() construct the supported V0 type shapes.
class TypeReference final {
public:
    // Creates a named scalar or model type reference.
    [[nodiscard]] static TypeReference createNamed(
        std::string name,
        source::SourceLocation location
    );

    // Creates a List type containing one element type.
    [[nodiscard]] static TypeReference createList(
        TypeReference elementType,
        source::SourceLocation location
    );

    // Returns whether this reference is named or a List.
    [[nodiscard]] TypeReferenceKind getKind() const noexcept;

    // Returns the scalar or model name for a named type.
    [[nodiscard]] const std::string& getName() const noexcept;

    // Returns the List element type or null for a named type.
    [[nodiscard]] const TypeReference* getElementType() const noexcept;

    // Returns the location where this type reference begins.
    [[nodiscard]] const source::SourceLocation& getLocation() const noexcept;

private:
    // Creates the internal representation for one type shape.
    TypeReference(
        TypeReferenceKind kind,
        std::string name,
        std::unique_ptr<TypeReference> elementType,
        source::SourceLocation location
    );

    TypeReferenceKind kind_;
    std::string name_;
    std::unique_ptr<TypeReference> elementType_;
    source::SourceLocation location_;
};

}
