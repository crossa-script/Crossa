#pragma once

#include <string>

namespace crossa::compiler::generators::kotlin {

// Stores one deterministic Kotlin file name and its complete generated content.
class KotlinGeneratedSource final {
public:
    // Creates one generated Kotlin source artifact.
    KotlinGeneratedSource(std::string fileName, std::string content);

    // Returns the deterministic Kotlin file name.
    [[nodiscard]] const std::string& getFileName() const noexcept;

    // Returns the complete canonical Kotlin source content.
    [[nodiscard]] const std::string& getContent() const noexcept;

private:
    std::string fileName_;
    std::string content_;
};

}
