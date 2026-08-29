#include "crossa/network/HttpHeader.h"

#include <utility>

using namespace std;

namespace crossa::network {

    // Creates one HTTP header pair.
    HttpHeader::HttpHeader(string name, string value)
        : name_(std::move(name)), value_(std::move(value)) {}

    // Returns the HTTP header name.
    const string& HttpHeader::getName() const noexcept {
        return name_;
    }

    // Returns the HTTP header value.
    const string& HttpHeader::getValue() const noexcept {
        return value_;
    }

}
