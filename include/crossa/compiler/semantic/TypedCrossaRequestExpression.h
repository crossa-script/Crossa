#pragma once

#include <memory>

#include "crossa/compiler/semantic/TypedExpression.h"

namespace crossa::compiler::semantic {

// Identifies the semantically validated HTTP method of a native request.
enum class SemanticHttpMethod {
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

// Owns a validated native request plan before platform-neutral IR lowering.
// URL, header/query maps, body, timeout, method, and response type are explicit.
class TypedCrossaRequestExpression final : public TypedExpression {
public:
    // Creates one complete typed native request expression.
    TypedCrossaRequestExpression(
        SemanticHttpMethod method,
        std::unique_ptr<TypedExpression> url,
        std::unique_ptr<TypedExpression> headers,
        std::unique_ptr<TypedExpression> customHeaders,
        std::unique_ptr<TypedExpression> queryParams,
        std::unique_ptr<TypedExpression> body,
        std::unique_ptr<TypedExpression> timeout,
        types::SemanticType responseType,
        source::SourceLocation location
    );

    // Returns the validated HTTP method.
    [[nodiscard]] SemanticHttpMethod getMethod() const noexcept;

    // Returns the validated URL construction expression.
    [[nodiscard]] const TypedExpression& getUrl() const noexcept;

    // Returns request headers or null when absent.
    [[nodiscard]] const TypedExpression* getHeaders() const noexcept;

    // Returns overriding custom headers or null when absent.
    [[nodiscard]] const TypedExpression* getCustomHeaders() const noexcept;

    // Returns query parameters or null when absent.
    [[nodiscard]] const TypedExpression* getQueryParams() const noexcept;

    // Returns the JSON body expression or null when absent.
    [[nodiscard]] const TypedExpression* getBody() const noexcept;

    // Returns the request timeout expression or null when absent.
    [[nodiscard]] const TypedExpression* getTimeout() const noexcept;

private:
    SemanticHttpMethod method_;
    std::unique_ptr<TypedExpression> url_;
    std::unique_ptr<TypedExpression> headers_;
    std::unique_ptr<TypedExpression> customHeaders_;
    std::unique_ptr<TypedExpression> queryParams_;
    std::unique_ptr<TypedExpression> body_;
    std::unique_ptr<TypedExpression> timeout_;
};

}
