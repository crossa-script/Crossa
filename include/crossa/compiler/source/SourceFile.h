#pragma once

#include <filesystem>
#include <string>

namespace crossa::compiler::source {

// Stores a Crossa source path and its complete source content.
// getPath() and getContent() expose both values to compiler stages.
class SourceFile final {
public:
    // Creates a source file value with its path and content.
    SourceFile(std::filesystem::path path, std::string content);

    // Returns the source file path.
    [[nodiscard]] const std::filesystem::path& getPath() const noexcept;

    // Returns the complete source file content.
    [[nodiscard]] const std::string& getContent() const noexcept;

private:
    std::filesystem::path path_;
    std::string content_;
};

}
