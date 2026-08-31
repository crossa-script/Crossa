#include "crossa/network/transport/CurlTransport.h"

#include <stdexcept>

using namespace std;

namespace crossa::network::transport {

    // Fails request execution when an Android artifact omits the TLS transport.
    class CurlTransport::Implementation final {};

    // Creates a transport placeholder for pure generated native runtime builds.
    CurlTransport::CurlTransport(size_t) : implementation_(make_unique<Implementation>()) {}

    // Releases the pure-runtime transport placeholder.
    CurlTransport::~CurlTransport() = default;

    // Rejects requests until a verified Android libcurl and TLS backend is linked.
    response::HttpResponse CurlTransport::execute(
        const request::PreparedRequest&,
        const runtime::RequestHandle&
    ) {
        throw runtime_error(
            "CrossaRequest requires the Android libcurl TLS transport package."
        );
    }

}
