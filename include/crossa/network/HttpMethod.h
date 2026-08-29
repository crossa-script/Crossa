#pragma once

#include <string_view>

namespace crossa::network {

// Identifies every HTTP request method executable by the network module.
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

// Converts HTTP method values into stable wire names.
// toString() is used by request transport and debug observability.
class HttpMethodUtils final {
public:
    // Returns the uppercase wire name of one HTTP method.
    [[nodiscard]] static std::string_view toString(HttpMethod method) noexcept;
};

}
