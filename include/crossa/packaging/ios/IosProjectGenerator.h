#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include "crossa/compiler/generators/swift/SwiftGeneratedSource.h"
#include "crossa/compiler/ir/Program.h"

namespace crossa::packaging::ios {

// Writes a self-contained Xcode framework project around generated Swift and C++ code.
// generate() emits build scripts, source, module headers, metadata, and archive inputs.
class IosProjectGenerator final {
public:
    // Writes the complete iOS framework build project to the selected output directory.
    void generate(
        const std::vector<compiler::generators::swift::SwiftGeneratedSource>&
            swiftSources,
        const std::vector<const compiler::ir::Program*>& programs,
        const std::filesystem::path& outputDirectory
    ) const;

    // Writes SwiftPM binary-package files for one archived XCFramework artifact.
    static void writeBinaryPackage(
        const std::filesystem::path& artifactRoot,
        const std::string& packageVersion,
        const std::optional<std::string>& packageBaseUrl
    );
};

}
