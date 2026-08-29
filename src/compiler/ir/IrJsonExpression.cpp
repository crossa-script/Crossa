#include "crossa/compiler/ir/IrJsonExpression.h"

#include <utility>

using namespace std;

namespace crossa::compiler::ir {

    // Creates one JSON number IR expression.
    IrJsonNumberExpression::IrJsonNumberExpression(
        string value,
        source::SourceLocation location
    )
        : IrExpression(
              IrExpressionKind::JsonNumber,
              types::SemanticType::createJson(),
              location
          ),
          value_(std::move(value)) {}

    // Returns the exact JSON number text.
    const string& IrJsonNumberExpression::getValue() const noexcept {
        return value_;
    }

    // Creates one JSON null IR expression.
    IrJsonNullExpression::IrJsonNullExpression(
        source::SourceLocation location
    )
        : IrExpression(
              IrExpressionKind::JsonNull,
              types::SemanticType::createJson(),
              location
          ) {}

    // Creates one lowered JSON object field.
    IrJsonObjectEntry::IrJsonObjectEntry(
        string key,
        unique_ptr<IrExpression> value,
        source::SourceLocation location
    )
        : key_(std::move(key)),
          value_(std::move(value)),
          location_(location) {}

    // Returns the JSON object key.
    const string& IrJsonObjectEntry::getKey() const noexcept {
        return key_;
    }

    // Returns the lowered field value expression.
    const IrExpression& IrJsonObjectEntry::getValue() const noexcept {
        return *value_;
    }

    // Returns where the source field originated.
    const source::SourceLocation&
    IrJsonObjectEntry::getLocation() const noexcept {
        return location_;
    }

    // Creates one JSON object construction expression.
    IrJsonObjectExpression::IrJsonObjectExpression(
        vector<IrJsonObjectEntry> entries,
        source::SourceLocation location
    )
        : IrExpression(
              IrExpressionKind::JsonObject,
              types::SemanticType::createJson(),
              location
          ),
          entries_(std::move(entries)) {}

    // Returns the ordered lowered JSON fields.
    const vector<IrJsonObjectEntry>&
    IrJsonObjectExpression::getEntries() const noexcept {
        return entries_;
    }

    // Creates one JSON array construction expression.
    IrJsonArrayExpression::IrJsonArrayExpression(
        vector<unique_ptr<IrExpression>> values,
        source::SourceLocation location
    )
        : IrExpression(
              IrExpressionKind::JsonArray,
              types::SemanticType::createJson(),
              location
          ),
          values_(std::move(values)) {}

    // Returns the ordered lowered JSON values.
    const vector<unique_ptr<IrExpression>>&
    IrJsonArrayExpression::getValues() const noexcept {
        return values_;
    }

}
