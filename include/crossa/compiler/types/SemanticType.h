#pragma once

#include <memory>
#include <string>

namespace crossa::compiler::types {

// Identifies every resolved type supported by CRA Language V0.
enum class SemanticTypeKind {
    Unit,
    Int,
    Long,
    Double,
    String,
    Bool,
    Json,
    Model,
    List
};

// Represents one fully resolved scalar, model, unit, or List type.
// Factory functions construct valid type shapes and format() names them.
class SemanticType final {
public:
    // Creates a deep copy of a resolved semantic type.
    SemanticType(const SemanticType& other);

    // Moves an existing resolved semantic type.
    SemanticType(SemanticType&& other) noexcept = default;

    // Replaces this type with a deep copy of another type.
    SemanticType& operator=(const SemanticType& other);

    // Replaces this type by moving another resolved type.
    SemanticType& operator=(SemanticType&& other) noexcept = default;

    // Creates the internal no-value function type.
    [[nodiscard]] static SemanticType createUnit();

    // Creates the built-in Int type.
    [[nodiscard]] static SemanticType createInt();

    // Creates the built-in Long type.
    [[nodiscard]] static SemanticType createLong();

    // Creates the built-in Double type.
    [[nodiscard]] static SemanticType createDouble();

    // Creates the built-in String type.
    [[nodiscard]] static SemanticType createString();

    // Creates the built-in Bool type.
    [[nodiscard]] static SemanticType createBool();

    // Creates the built-in Json type.
    [[nodiscard]] static SemanticType createJson();

    // Creates a resolved named model type.
    [[nodiscard]] static SemanticType createModel(std::string name);

    // Creates a resolved List type containing one element type.
    [[nodiscard]] static SemanticType createList(SemanticType elementType);

    // Returns the resolved semantic type category.
    [[nodiscard]] SemanticTypeKind getKind() const noexcept;

    // Returns the model name or an empty string for non-model types.
    [[nodiscard]] const std::string& getModelName() const noexcept;

    // Returns the List element type or null for non-list types.
    [[nodiscard]] const SemanticType* getElementType() const noexcept;

    // Returns a stable readable representation of this type.
    [[nodiscard]] std::string format() const;

    // Returns whether two resolved semantic types are equivalent.
    [[nodiscard]] bool operator==(const SemanticType& other) const noexcept;

    // Returns whether two resolved semantic types differ.
    [[nodiscard]] bool operator!=(const SemanticType& other) const noexcept;

private:
    // Creates one valid resolved type representation.
    SemanticType(
        SemanticTypeKind kind,
        std::string modelName,
        std::unique_ptr<SemanticType> elementType
    );

    SemanticTypeKind kind_;
    std::string modelName_;
    std::unique_ptr<SemanticType> elementType_;
};

}
