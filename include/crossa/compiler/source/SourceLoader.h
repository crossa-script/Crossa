#pragma once

#include <filesystem>

#include "crossa/compiler/source/SourceFile.h"
#include "crossa/utils/Log.h"

namespace crossa::compiler::source {

// Validates and loads Crossa source files from disk.
// load() enforces the .cra execution boundary.
class SourceLoader final {
public:
    // Loads a .cra file and returns its path and complete content.
    [[nodiscard]] static SourceFile load(
        const std::filesystem::path& path,
        const utils::Log& log
    );

private:
    // Checks whether the path uses the supported .cra extension.
    [[nodiscard]] static bool hasSupportedExtension(
        const std::filesystem::path& path
    ) noexcept;
};

}
