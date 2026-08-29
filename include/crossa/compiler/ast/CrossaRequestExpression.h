#pragma once

#include <memory>
#include <string>
#include <vector>

#include "crossa/compiler/ast/Expression.h"

namespace crossa::compiler::ast {

// Identifies every standard HTTP method supported by CrossaRequest.
enum class HttpMethod {
    Get,
    Post,
    Put,
    Patch,
    Delete,
    Head,
    Options,
    Trace,
    Connect
};

// Represents one HTTP method literal inside a CrossaRequest block.
// getMethod() exposes the parsed method without runtime string lookup.
class HttpMethodLiteralExpression final : public Expression {
public:
    // Creates one HTTP method literal expression.
    HttpMethodLiteralExpression(
        HttpMethod method,
        source::SourceLocation location
    ) noexcept;

    // Returns the parsed HTTP method.
    [[nodiscard]] HttpMethod getMethod() const noexcept;

private:
    HttpMethod method_;
};

// Stores one named CrossaRequest builder entry and its source value.
// getName() and getValue() expose syntax for request semantic validation.
class CrossaRequestEntry final {
public:
    // Creates one named request builder entry.
    CrossaRequestEntry(
        std::string name,
        std::unique_ptr<Expression> value,
        source::SourceLocation location
    );

    // Returns the request entry name.
    [[nodiscard]] const std::string& getName() const noexcept;

    // Returns the request entry source expression.
    [[nodiscard]] const Expression& getValue() const noexcept;

    // Returns where the request entry begins.
    [[nodiscard]] const source::SourceLocation& getLocation() const noexcept;

private:
    std::string name_;
    std::unique_ptr<Expression> value_;
    source::SourceLocation location_;
};

// Represents one native request builder block before semantic validation.
// getEntries() supplies URL, method, headers, query, path, body, and timeout.
class CrossaRequestExpression final : public Expression {
public:
    // Creates a request expression from ordered builder entries.
    CrossaRequestExpression(
        std::vector<CrossaRequestEntry> entries,
        source::SourceLocation location
    );

    // Returns the ordered request builder entries.
    [[nodiscard]] const std::vector<CrossaRequestEntry>&
    getEntries() const noexcept;

private:
    std::vector<CrossaRequestEntry> entries_;
};

}
