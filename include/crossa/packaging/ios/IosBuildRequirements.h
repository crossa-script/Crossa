#pragma once

#include <string>

namespace crossa::packaging::ios {

// Defines centralized iOS framework build requirements and pinned dependencies.
class IosBuildRequirements final {
public:
    // Returns the centralized minimum deployment target for produced artifacts.
    [[nodiscard]] static std::string minimumDeploymentTarget();

    // Returns the pinned curl version shared with generated Android projects.
    [[nodiscard]] static std::string curlVersion();

    // Returns the verified official curl source archive URL.
    [[nodiscard]] static std::string curlArchiveUrl();

    // Returns the SHA-256 digest for the pinned curl archive.
    [[nodiscard]] static std::string curlArchiveSha256();
};

}
