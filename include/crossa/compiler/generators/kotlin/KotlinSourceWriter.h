#pragma once

#include <cstddef>
#include <string>

namespace crossa::compiler::generators::kotlin {

// Builds canonical Kotlin source with deterministic indentation and line endings.
class KotlinSourceWriter final {
public:
    // Creates an empty writer with the requested initial output capacity.
    explicit KotlinSourceWriter(std::size_t initialCapacity = 1024);

    // Writes one complete indented line.
    void writeLine(const std::string& line = "");

    // Begins one Kotlin block after writing its header.
    void beginBlock(const std::string& header);

    // Ends the current Kotlin block.
    void endBlock();

    // Increases indentation for nested Kotlin content.
    void indent();

    // Decreases indentation after nested Kotlin content.
    void dedent();

    // Returns the assembled source with exactly one trailing newline.
    [[nodiscard]] const std::string& getSource() const noexcept;

private:
    std::string source_;
    std::size_t indentationLevel_;
};

}
