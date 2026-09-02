#pragma once

#include <optional>
#include <string>

namespace crossa::runtime {

// Identifies the stable subsystem that produced a native Crossa failure.
enum class CrossaErrorDomain {
    Http = 0,
    Transport = 1,
    Serialization = 2,
    Runtime = 3,
    Cancellation = 4
};

// Identifies every stable native failure category exposed by CrossaState.
enum class CrossaErrorCode {
    HttpStatus = 0,
    Timeout = 1,
    Connection = 2,
    Tls = 3,
    InvalidJson = 4,
    ResponseTypeMismatch = 5,
    Cancellation = 6,
    Runtime = 7
};

// Carries a stable native error category, readable cause, and optional metadata.
// Factory functions preserve consistent domains and retryability decisions.
class CrossaError final {
public:
    // Creates an HTTP status failure and preserves the returned status code.
    [[nodiscard]] static CrossaError httpStatus(long statusCode);

    // Creates a request timeout failure.
    [[nodiscard]] static CrossaError timeout(std::string message);

    // Creates a connection or name-resolution failure.
    [[nodiscard]] static CrossaError connection(
        std::string message,
        std::optional<long> nativeCode = std::nullopt
    );

    // Creates a TLS negotiation or certificate failure.
    [[nodiscard]] static CrossaError tls(
        std::string message,
        std::optional<long> nativeCode = std::nullopt
    );

    // Creates a malformed or bounded JSON parsing failure.
    [[nodiscard]] static CrossaError invalidJson(std::string message);

    // Creates a response-to-semantic-type mismatch failure.
    [[nodiscard]] static CrossaError responseTypeMismatch(
        std::string message
    );

    // Creates an explicit native cancellation result.
    [[nodiscard]] static CrossaError cancellation(
        std::string message = "Native operation was cancelled."
    );

    // Creates an internal runtime execution failure.
    [[nodiscard]] static CrossaError runtime(std::string message);

    // Returns the stable error domain.
    [[nodiscard]] CrossaErrorDomain getDomain() const noexcept;

    // Returns the stable error code.
    [[nodiscard]] CrossaErrorCode getCode() const noexcept;

    // Returns the native diagnostic message.
    [[nodiscard]] const std::string& getMessage() const noexcept;

    // Returns whether retrying may recover without changing the request.
    [[nodiscard]] bool isRetryable() const noexcept;

    // Returns the HTTP status when the server produced one.
    [[nodiscard]] const std::optional<long>& getHttpStatus() const noexcept;

    // Returns the underlying transport code when one is available.
    [[nodiscard]] const std::optional<long>& getNativeCode() const noexcept;

    // Returns a stable diagnostic representation for logs and bindings.
    [[nodiscard]] std::string format() const;

    // Returns the stable symbolic domain name.
    [[nodiscard]] static std::string domainName(
        CrossaErrorDomain domain
    );

    // Returns the stable symbolic error-code name.
    [[nodiscard]] static std::string codeName(CrossaErrorCode code);

private:
    // Creates one fully structured immutable error value.
    CrossaError(
        CrossaErrorDomain domain,
        CrossaErrorCode code,
        std::string message,
        bool retryable,
        std::optional<long> httpStatus,
        std::optional<long> nativeCode
    );

    CrossaErrorDomain domain_;
    CrossaErrorCode code_;
    std::string message_;
    bool retryable_;
    std::optional<long> httpStatus_;
    std::optional<long> nativeCode_;
};

}
