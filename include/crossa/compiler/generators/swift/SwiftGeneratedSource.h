#pragma once

#include <string>

namespace crossa::compiler::generators::swift {

// Stores one deterministic Swift relative path and its generated source text.
class SwiftGeneratedSource final {
public:
    // Creates one generated Swift source artifact.
    SwiftGeneratedSource(std::string fileName, std::string content);

    // Returns the deterministic path relative to the generated Swift root.
    [[nodiscard]] const std::string& getFileName() const noexcept;

    // Returns the complete canonical Swift source text.
    [[nodiscard]] const std::string& getContent() const noexcept;

private:
    std::string fileName_;
    std::string content_;
};

}
