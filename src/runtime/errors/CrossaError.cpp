#include "crossa/runtime/errors/CrossaError.h"

#include <utility>

using namespace std;

namespace crossa::runtime {

    // Creates an HTTP status failure and preserves the returned status code.
    CrossaError CrossaError::httpStatus(long statusCode) {
        const bool retryable = statusCode == 408 || statusCode == 425 ||
            statusCode == 429 || statusCode >= 500;
        return CrossaError(
            CrossaErrorDomain::Http,
            CrossaErrorCode::HttpStatus,
            "HTTP request failed with status " + to_string(statusCode) + ".",
            retryable,
            statusCode,
            nullopt
        );
    }

    // Creates a request timeout failure.
    CrossaError CrossaError::timeout(string message) {
        return CrossaError(
            CrossaErrorDomain::Transport,
            CrossaErrorCode::Timeout,
            std::move(message),
            true,
            nullopt,
            nullopt
        );
    }

    // Creates a connection or name-resolution failure.
    CrossaError CrossaError::connection(
        string message,
        optional<long> nativeCode
    ) {
        return CrossaError(
            CrossaErrorDomain::Transport,
            CrossaErrorCode::Connection,
            std::move(message),
            true,
            nullopt,
            nativeCode
        );
    }

    // Creates a TLS negotiation or certificate failure.
    CrossaError CrossaError::tls(
        string message,
        optional<long> nativeCode
    ) {
        return CrossaError(
            CrossaErrorDomain::Transport,
            CrossaErrorCode::Tls,
            std::move(message),
            false,
            nullopt,
            nativeCode
        );
    }

    // Creates a malformed or bounded JSON parsing failure.
    CrossaError CrossaError::invalidJson(string message) {
        return CrossaError(
            CrossaErrorDomain::Serialization,
            CrossaErrorCode::InvalidJson,
            std::move(message),
            false,
            nullopt,
            nullopt
        );
    }

    // Creates a response-to-semantic-type mismatch failure.
    CrossaError CrossaError::responseTypeMismatch(string message) {
        return CrossaError(
            CrossaErrorDomain::Serialization,
            CrossaErrorCode::ResponseTypeMismatch,
            std::move(message),
            false,
            nullopt,
            nullopt
        );
    }

    // Creates an explicit native cancellation result.
    CrossaError CrossaError::cancellation(string message) {
        return CrossaError(
            CrossaErrorDomain::Cancellation,
            CrossaErrorCode::Cancellation,
            std::move(message),
            false,
            nullopt,
            nullopt
        );
    }

    // Creates an internal runtime execution failure.
    CrossaError CrossaError::runtime(string message) {
        return CrossaError(
            CrossaErrorDomain::Runtime,
            CrossaErrorCode::Runtime,
            std::move(message),
            false,
            nullopt,
            nullopt
        );
    }

    // Returns the stable error domain.
    CrossaErrorDomain CrossaError::getDomain() const noexcept {
        return domain_;
    }

    // Returns the stable error code.
    CrossaErrorCode CrossaError::getCode() const noexcept {
        return code_;
    }

    // Returns the native diagnostic message.
    const string& CrossaError::getMessage() const noexcept {
        return message_;
    }

    // Returns whether retrying may recover without changing the request.
    bool CrossaError::isRetryable() const noexcept {
        return retryable_;
    }

    // Returns the HTTP status when the server produced one.
    const optional<long>& CrossaError::getHttpStatus() const noexcept {
        return httpStatus_;
    }

    // Returns the underlying transport code when one is available.
    const optional<long>& CrossaError::getNativeCode() const noexcept {
        return nativeCode_;
    }

    // Returns a stable diagnostic representation for logs and bindings.
    string CrossaError::format() const {
        string value = domainName(domain_) + "/" + codeName(code_) +
            ": " + message_;
        if (httpStatus_.has_value()) {
            value += " httpStatus=" + to_string(*httpStatus_);
        }
        if (nativeCode_.has_value()) {
            value += " nativeCode=" + to_string(*nativeCode_);
        }
        value += retryable_ ? " retryable=true" : " retryable=false";
        return value;
    }

    // Returns the stable symbolic domain name.
    string CrossaError::domainName(CrossaErrorDomain domain) {
        switch (domain) {
            case CrossaErrorDomain::Http:
                return "http";
            case CrossaErrorDomain::Transport:
                return "transport";
            case CrossaErrorDomain::Serialization:
                return "serialization";
            case CrossaErrorDomain::Runtime:
                return "runtime";
            case CrossaErrorDomain::Cancellation:
                return "cancellation";
        }
        return "runtime";
    }

    // Returns the stable symbolic error-code name.
    string CrossaError::codeName(CrossaErrorCode code) {
        switch (code) {
            case CrossaErrorCode::HttpStatus:
                return "http_status";
            case CrossaErrorCode::Timeout:
                return "timeout";
            case CrossaErrorCode::Connection:
                return "connection";
            case CrossaErrorCode::Tls:
                return "tls";
            case CrossaErrorCode::InvalidJson:
                return "invalid_json";
            case CrossaErrorCode::ResponseTypeMismatch:
                return "response_type_mismatch";
            case CrossaErrorCode::Cancellation:
                return "cancellation";
            case CrossaErrorCode::Runtime:
                return "runtime";
        }
        return "runtime";
    }

    // Creates one fully structured immutable error value.
    CrossaError::CrossaError(
        CrossaErrorDomain domain,
        CrossaErrorCode code,
        string message,
        bool retryable,
        optional<long> httpStatus,
        optional<long> nativeCode
    )
        : domain_(domain),
          code_(code),
          message_(std::move(message)),
          retryable_(retryable),
          httpStatus_(httpStatus),
          nativeCode_(nativeCode) {}

}
