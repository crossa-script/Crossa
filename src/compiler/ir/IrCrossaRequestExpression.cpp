#include "crossa/compiler/ir/IrCrossaRequestExpression.h"

#include <utility>

using namespace std;

namespace crossa::compiler::ir {

    // Creates one complete native request IR expression.
    IrCrossaRequestExpression::IrCrossaRequestExpression(
        IrHttpMethod method,
        unique_ptr<IrExpression> url,
        unique_ptr<IrExpression> headers,
        unique_ptr<IrExpression> customHeaders,
        unique_ptr<IrExpression> queryParams,
        unique_ptr<IrExpression> body,
        unique_ptr<IrExpression> timeout,
        unique_ptr<IrExpression> retryPolicy,
        unique_ptr<IrExpression> authentication,
        unique_ptr<IrExpression> multipart,
        unique_ptr<IrExpression> uploadProgress,
        unique_ptr<IrExpression> downloadStreaming,
        unique_ptr<IrExpression> coalesce,
        unique_ptr<IrExpression> proxy,
        unique_ptr<IrExpression> certificatePolicy,
        unique_ptr<IrExpression> telemetry,
        types::SemanticType responseType,
        source::SourceLocation location
    )
        : IrExpression(
              IrExpressionKind::CrossaRequest,
              std::move(responseType),
              location
          ),
          method_(method),
          url_(std::move(url)),
          headers_(std::move(headers)),
          customHeaders_(std::move(customHeaders)),
          queryParams_(std::move(queryParams)),
          body_(std::move(body)),
          timeout_(std::move(timeout)),
          retryPolicy_(std::move(retryPolicy)),
          authentication_(std::move(authentication)),
          multipart_(std::move(multipart)),
          uploadProgress_(std::move(uploadProgress)),
          downloadStreaming_(std::move(downloadStreaming)),
          coalesce_(std::move(coalesce)),
          proxy_(std::move(proxy)),
          certificatePolicy_(std::move(certificatePolicy)),
          telemetry_(std::move(telemetry)) {}

    // Returns the lowered HTTP method.
    IrHttpMethod IrCrossaRequestExpression::getMethod() const noexcept {
        return method_;
    }

    // Returns the lowered URL construction expression.
    const IrExpression& IrCrossaRequestExpression::getUrl() const noexcept {
        return *url_;
    }

    // Returns request headers or null when absent.
    const IrExpression*
    IrCrossaRequestExpression::getHeaders() const noexcept {
        return headers_.get();
    }

    // Returns overriding custom headers or null when absent.
    const IrExpression*
    IrCrossaRequestExpression::getCustomHeaders() const noexcept {
        return customHeaders_.get();
    }

    // Returns query parameters or null when absent.
    const IrExpression*
    IrCrossaRequestExpression::getQueryParams() const noexcept {
        return queryParams_.get();
    }

    // Returns the JSON body or null when absent.
    const IrExpression* IrCrossaRequestExpression::getBody() const noexcept {
        return body_.get();
    }

    // Returns the timeout expression or null when absent.
    const IrExpression* IrCrossaRequestExpression::getTimeout() const noexcept {
        return timeout_.get();
    }

    const IrExpression* IrCrossaRequestExpression::getRetryPolicy() const noexcept {
        return retryPolicy_.get();
    }

    const IrExpression* IrCrossaRequestExpression::getAuthentication() const noexcept {
        return authentication_.get();
    }

    const IrExpression* IrCrossaRequestExpression::getMultipart() const noexcept {
        return multipart_.get();
    }

    const IrExpression* IrCrossaRequestExpression::getUploadProgress() const noexcept {
        return uploadProgress_.get();
    }

    const IrExpression* IrCrossaRequestExpression::getDownloadStreaming() const noexcept {
        return downloadStreaming_.get();
    }

    const IrExpression* IrCrossaRequestExpression::getCoalesce() const noexcept {
        return coalesce_.get();
    }

    const IrExpression* IrCrossaRequestExpression::getProxy() const noexcept {
        return proxy_.get();
    }

    const IrExpression* IrCrossaRequestExpression::getCertificatePolicy() const noexcept {
        return certificatePolicy_.get();
    }

    const IrExpression* IrCrossaRequestExpression::getTelemetry() const noexcept {
        return telemetry_.get();
    }

}
