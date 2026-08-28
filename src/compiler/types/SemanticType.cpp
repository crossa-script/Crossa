#include "crossa/compiler/types/SemanticType.h"

#include <utility>

using namespace std;

namespace crossa::compiler::types {

    // Creates a deep copy of a resolved semantic type.
    SemanticType::SemanticType(const SemanticType& other)
        : kind_(other.kind_),
          modelName_(other.modelName_),
          elementType_(
              other.elementType_ == nullptr
                  ? nullptr
                  : make_unique<SemanticType>(*other.elementType_)
          ) {}

    // Replaces this type with a deep copy of another type.
    SemanticType& SemanticType::operator=(const SemanticType& other) {
        if (this == &other) {
            return *this;
        }

        kind_ = other.kind_;
        modelName_ = other.modelName_;
        elementType_ = other.elementType_ == nullptr
            ? nullptr
            : make_unique<SemanticType>(*other.elementType_);
        return *this;
    }

    // Creates the internal no-value function type.
    SemanticType SemanticType::createUnit() {
        return SemanticType(SemanticTypeKind::Unit, "", nullptr);
    }

    // Creates the built-in Int type.
    SemanticType SemanticType::createInt() {
        return SemanticType(SemanticTypeKind::Int, "", nullptr);
    }

    // Creates the built-in String type.
    SemanticType SemanticType::createString() {
        return SemanticType(SemanticTypeKind::String, "", nullptr);
    }

    // Creates the built-in Bool type.
    SemanticType SemanticType::createBool() {
        return SemanticType(SemanticTypeKind::Bool, "", nullptr);
    }

    // Creates a resolved named model type.
    SemanticType SemanticType::createModel(string name) {
        return SemanticType(
            SemanticTypeKind::Model,
            std::move(name),
            nullptr
        );
    }

    // Creates a resolved List type containing one element type.
    SemanticType SemanticType::createList(SemanticType elementType) {
        return SemanticType(
            SemanticTypeKind::List,
            "",
            make_unique<SemanticType>(std::move(elementType))
        );
    }

    // Returns the resolved semantic type category.
    SemanticTypeKind SemanticType::getKind() const noexcept {
        return kind_;
    }

    // Returns the model name or an empty string for non-model types.
    const string& SemanticType::getModelName() const noexcept {
        return modelName_;
    }

    // Returns the List element type or null for non-list types.
    const SemanticType* SemanticType::getElementType() const noexcept {
        return elementType_.get();
    }

    // Returns a stable readable representation of this type.
    string SemanticType::format() const {
        switch (kind_) {
            case SemanticTypeKind::Unit:
                return "Unit";
            case SemanticTypeKind::Int:
                return "Int";
            case SemanticTypeKind::String:
                return "String";
            case SemanticTypeKind::Bool:
                return "Bool";
            case SemanticTypeKind::Model:
                return modelName_;
            case SemanticTypeKind::List:
                return elementType_ == nullptr
                    ? "List<?>"
                    : "List<" + elementType_->format() + ">";
        }

        return "Unknown";
    }

    // Returns whether two resolved semantic types are equivalent.
    bool SemanticType::operator==(const SemanticType& other) const noexcept {
        if (kind_ != other.kind_ || modelName_ != other.modelName_) {
            return false;
        }

        if (elementType_ == nullptr || other.elementType_ == nullptr) {
            return elementType_ == nullptr && other.elementType_ == nullptr;
        }

        return *elementType_ == *other.elementType_;
    }

    // Returns whether two resolved semantic types differ.
    bool SemanticType::operator!=(const SemanticType& other) const noexcept {
        return !(*this == other);
    }

    // Creates one valid resolved type representation.
    SemanticType::SemanticType(
        SemanticTypeKind kind,
        string modelName,
        unique_ptr<SemanticType> elementType
    )
        : kind_(kind),
          modelName_(std::move(modelName)),
          elementType_(std::move(elementType)) {}

}
