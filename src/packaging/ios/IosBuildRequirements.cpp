#include "crossa/packaging/ios/IosBuildRequirements.h"

using namespace std;

namespace crossa::packaging::ios {

    // Returns the centralized minimum deployment target for produced artifacts.
    string IosBuildRequirements::minimumDeploymentTarget() {
        return "13.0";
    }

    // Returns the pinned curl version shared with generated Android projects.
    string IosBuildRequirements::curlVersion() {
        return "8.12.1";
    }

    // Returns the verified official curl source archive URL.
    string IosBuildRequirements::curlArchiveUrl() {
        return "https://curl.se/download/curl-8.12.1.tar.xz";
    }

    // Returns the SHA-256 digest for the pinned curl archive.
    string IosBuildRequirements::curlArchiveSha256() {
        return "0341f1ed97a26c811abaebd37d62b833956792b7607ea3f15d001613c76de202";
    }

}
