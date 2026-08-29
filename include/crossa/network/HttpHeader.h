#pragma once

#include <string>

namespace crossa::network {

// Owns one validated HTTP header name and value pair.
// getName() and getValue() expose immutable transport metadata.
class HttpHeader final {
public:
    // Creates one HTTP header pair.
    HttpHeader(std::string name, std::string value);

    // Returns the HTTP header name.
    [[nodiscard]] const std::string& getName() const noexcept;

    // Returns the HTTP header value.
    [[nodiscard]] const std::string& getValue() const noexcept;

private:
    std::string name_;
    std::string value_;
};

}
