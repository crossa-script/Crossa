#include "crossa/network/HttpMethod.h"

using namespace std;

namespace crossa::network {

    // Returns the uppercase wire name of one HTTP method.
    string_view HttpMethodUtils::toString(HttpMethod method) noexcept {
        switch (method) {
            case HttpMethod::Get:
                return "GET";
            case HttpMethod::Post:
                return "POST";
            case HttpMethod::Put:
                return "PUT";
            case HttpMethod::Patch:
                return "PATCH";
            case HttpMethod::Delete:
                return "DELETE";
            case HttpMethod::Head:
                return "HEAD";
            case HttpMethod::Options:
                return "OPTIONS";
            case HttpMethod::Trace:
                return "TRACE";
            case HttpMethod::Connect:
                return "CONNECT";
        }
        return "GET";
    }

}
