#include "crossa/compiler/semantic/TypedCrossaRequestExpression.h"

#include <utility>

using namespace std;

namespace crossa::compiler::semantic {

    // Creates one complete typed native request expression.
    TypedCrossaRequestExpression::TypedCrossaRequestExpression(
        SemanticHttpMethod method,
        unique_ptr<TypedExpression> url,
        unique_ptr<TypedExpression> headers,
        unique_ptr<TypedExpression> customHeaders,
        unique_ptr<TypedExpression> queryParams,
        unique_ptr<TypedExpression> body,
        unique_ptr<TypedExpression> timeout,
        types::SemanticType responseType,
        source::SourceLocation location
    )
        : TypedExpression(
              TypedExpressionKind::CrossaRequest,
              std::move(responseType),
              location
          ),
          method_(method),
          url_(std::move(url)),
          headers_(std::move(headers)),
          customHeaders_(std::move(customHeaders)),
          queryParams_(std::move(queryParams)),
          body_(std::move(body)),
          timeout_(std::move(timeout)) {}

    // Returns the validated HTTP method.
    SemanticHttpMethod
    TypedCrossaRequestExpression::getMethod() const noexcept {
        return method_;
    }

    // Returns the validated URL construction expression.
    const TypedExpression& TypedCrossaRequestExpression::getUrl() const noexcept {
        return *url_;
    }

    // Returns request headers or null when absent.
    const TypedExpression*
    TypedCrossaRequestExpression::getHeaders() const noexcept {
        return headers_.get();
    }

    // Returns overriding custom headers or null when absent.
    const TypedExpression*
    TypedCrossaRequestExpression::getCustomHeaders() const noexcept {
        return customHeaders_.get();
    }

    // Returns query parameters or null when absent.
    const TypedExpression*
    TypedCrossaRequestExpression::getQueryParams() const noexcept {
        return queryParams_.get();
    }

    // Returns the JSON body expression or null when absent.
    const TypedExpression*
    TypedCrossaRequestExpression::getBody() const noexcept {
        return body_.get();
    }

    // Returns the request timeout expression or null when absent.
    const TypedExpression*
    TypedCrossaRequestExpression::getTimeout() const noexcept {
        return timeout_.get();
    }

}
