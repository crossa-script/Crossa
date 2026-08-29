#pragma once

#include <memory>
#include <string>
#include <vector>

#include "crossa/compiler/ir/IrExpression.h"

namespace crossa::compiler::ir {

// Represents an exact JSON decimal or exponent number in Crossa IR.
// getValue() supplies deterministic request serialization text.
class IrJsonNumberExpression final : public IrExpression {
public:
    // Creates one JSON number IR expression.
    IrJsonNumberExpression(
        std::string value,
        source::SourceLocation location
    );

    // Returns the exact JSON number text.
    [[nodiscard]] const std::string& getValue() const noexcept;

private:
    std::string value_;
};

// Represents the JSON null value in platform-neutral IR.
// Its inherited result type is Json.
class IrJsonNullExpression final : public IrExpression {
public:
    // Creates one JSON null IR expression.
    explicit IrJsonNullExpression(source::SourceLocation location);
};

// Stores one ordered lowered JSON object field.
// getKey() and getValue() expose deterministic JSON construction.
class IrJsonObjectEntry final {
public:
    // Creates one lowered JSON object field.
    IrJsonObjectEntry(
        std::string key,
        std::unique_ptr<IrExpression> value,
        source::SourceLocation location
    );

    // Returns the JSON object key.
    [[nodiscard]] const std::string& getKey() const noexcept;

    // Returns the lowered field value expression.
    [[nodiscard]] const IrExpression& getValue() const noexcept;

    // Returns where the source field originated.
    [[nodiscard]] const source::SourceLocation& getLocation() const noexcept;

private:
    std::string key_;
    std::unique_ptr<IrExpression> value_;
    source::SourceLocation location_;
};

// Represents an ordered JSON object construction plan in IR.
// getEntries() supplies fields for native evaluation.
class IrJsonObjectExpression final : public IrExpression {
public:
    // Creates one JSON object construction expression.
    IrJsonObjectExpression(
        std::vector<IrJsonObjectEntry> entries,
        source::SourceLocation location
    );

    // Returns the ordered lowered JSON fields.
    [[nodiscard]] const std::vector<IrJsonObjectEntry>&
    getEntries() const noexcept;

private:
    std::vector<IrJsonObjectEntry> entries_;
};

// Represents an ordered JSON array construction plan in IR.
// getValues() supplies items for native evaluation.
class IrJsonArrayExpression final : public IrExpression {
public:
    // Creates one JSON array construction expression.
    IrJsonArrayExpression(
        std::vector<std::unique_ptr<IrExpression>> values,
        source::SourceLocation location
    );

    // Returns the ordered lowered JSON values.
    [[nodiscard]] const std::vector<std::unique_ptr<IrExpression>>&
    getValues() const noexcept;

private:
    std::vector<std::unique_ptr<IrExpression>> values_;
};

}
