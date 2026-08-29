#include "crossa/compiler/ast/CrossaRequestExpression.h"

#include <utility>

using namespace std;

namespace crossa::compiler::ast {

    // Creates one HTTP method literal expression.
    HttpMethodLiteralExpression::HttpMethodLiteralExpression(
        HttpMethod method,
        source::SourceLocation location
    ) noexcept
        : Expression(ExpressionKind::HttpMethod, location), method_(method) {}

    // Returns the parsed HTTP method.
    HttpMethod HttpMethodLiteralExpression::getMethod() const noexcept {
        return method_;
    }

    // Creates one named request builder entry.
    CrossaRequestEntry::CrossaRequestEntry(
        string name,
        unique_ptr<Expression> value,
        source::SourceLocation location
    )
        : name_(std::move(name)),
          value_(std::move(value)),
          location_(location) {}

    // Returns the request entry name.
    const string& CrossaRequestEntry::getName() const noexcept {
        return name_;
    }

    // Returns the request entry source expression.
    const Expression& CrossaRequestEntry::getValue() const noexcept {
        return *value_;
    }

    // Returns where the request entry begins.
    const source::SourceLocation&
    CrossaRequestEntry::getLocation() const noexcept {
        return location_;
    }

    // Creates a request expression from ordered builder entries.
    CrossaRequestExpression::CrossaRequestExpression(
        vector<CrossaRequestEntry> entries,
        source::SourceLocation location
    )
        : Expression(ExpressionKind::CrossaRequest, location),
          entries_(std::move(entries)) {}

    // Returns the ordered request builder entries.
    const vector<CrossaRequestEntry>&
    CrossaRequestExpression::getEntries() const noexcept {
        return entries_;
    }

}
