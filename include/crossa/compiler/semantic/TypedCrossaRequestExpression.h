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
        std::unique_ptr<TypedExpression> retryPolicy,
        std::unique_ptr<TypedExpression> authentication,
        std::unique_ptr<TypedExpression> multipart,
        std::unique_ptr<TypedExpression> uploadProgress,
        std::unique_ptr<TypedExpression> downloadStreaming,
        std::unique_ptr<TypedExpression> coalesce,
        std::unique_ptr<TypedExpression> proxy,
        std::unique_ptr<TypedExpression> certificatePolicy,
        std::unique_ptr<TypedExpression> telemetry,
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

    [[nodiscard]] const TypedExpression* getRetryPolicy() const noexcept;

    [[nodiscard]] const TypedExpression* getAuthentication() const noexcept;

    [[nodiscard]] const TypedExpression* getMultipart() const noexcept;

    [[nodiscard]] const TypedExpression* getUploadProgress() const noexcept;

    [[nodiscard]] const TypedExpression* getDownloadStreaming() const noexcept;

    [[nodiscard]] const TypedExpression* getCoalesce() const noexcept;

    [[nodiscard]] const TypedExpression* getProxy() const noexcept;

    [[nodiscard]] const TypedExpression* getCertificatePolicy() const noexcept;

    [[nodiscard]] const TypedExpression* getTelemetry() const noexcept;

private:
    SemanticHttpMethod method_;
    std::unique_ptr<TypedExpression> url_;
    std::unique_ptr<TypedExpression> headers_;
    std::unique_ptr<TypedExpression> customHeaders_;
    std::unique_ptr<TypedExpression> queryParams_;
    std::unique_ptr<TypedExpression> body_;
    std::unique_ptr<TypedExpression> timeout_;
    std::unique_ptr<TypedExpression> retryPolicy_;
    std::unique_ptr<TypedExpression> authentication_;
    std::unique_ptr<TypedExpression> multipart_;
    std::unique_ptr<TypedExpression> uploadProgress_;
    std::unique_ptr<TypedExpression> downloadStreaming_;
    std::unique_ptr<TypedExpression> coalesce_;
    std::unique_ptr<TypedExpression> proxy_;
    std::unique_ptr<TypedExpression> certificatePolicy_;
    std::unique_ptr<TypedExpression> telemetry_;
};

}
