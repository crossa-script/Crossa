#pragma once

#include <filesystem>
#include <vector>

namespace crossa::cli {

// Finds deterministic executable Crossa sources without parsing their contents.
class ProjectSourceDiscovery final {
public:
    // Returns sorted non-configuration .cra files under a project directory.
    [[nodiscard]] static std::vector<std::filesystem::path> find(
        const std::filesystem::path& projectDirectory
    );
};

}
