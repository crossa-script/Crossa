#pragma once

#include <memory>

#include "crossa/compiler/ir/IrExpression.h"

namespace crossa::compiler::ir {

// Identifies every HTTP method encoded in a native request IR plan.
enum class IrHttpMethod {
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

// Owns one platform-neutral native request plan and expected response type.
// Its accessors expose URL, maps, body, timeout, and method to the runtime.
class IrCrossaRequestExpression final : public IrExpression {
public:
    // Creates one complete native request IR expression.
    IrCrossaRequestExpression(
        IrHttpMethod method,
        std::unique_ptr<IrExpression> url,
        std::unique_ptr<IrExpression> headers,
        std::unique_ptr<IrExpression> customHeaders,
        std::unique_ptr<IrExpression> queryParams,
        std::unique_ptr<IrExpression> body,
        std::unique_ptr<IrExpression> timeout,
        types::SemanticType responseType,
        source::SourceLocation location
    );

    // Returns the lowered HTTP method.
    [[nodiscard]] IrHttpMethod getMethod() const noexcept;

    // Returns the lowered URL construction expression.
    [[nodiscard]] const IrExpression& getUrl() const noexcept;

    // Returns request headers or null when absent.
    [[nodiscard]] const IrExpression* getHeaders() const noexcept;

    // Returns overriding custom headers or null when absent.
    [[nodiscard]] const IrExpression* getCustomHeaders() const noexcept;

    // Returns query parameters or null when absent.
    [[nodiscard]] const IrExpression* getQueryParams() const noexcept;

    // Returns the JSON body or null when absent.
    [[nodiscard]] const IrExpression* getBody() const noexcept;

    // Returns the timeout expression or null when absent.
    [[nodiscard]] const IrExpression* getTimeout() const noexcept;

private:
    IrHttpMethod method_;
    std::unique_ptr<IrExpression> url_;
    std::unique_ptr<IrExpression> headers_;
    std::unique_ptr<IrExpression> customHeaders_;
    std::unique_ptr<IrExpression> queryParams_;
    std::unique_ptr<IrExpression> body_;
    std::unique_ptr<IrExpression> timeout_;
};

}
