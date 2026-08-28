#pragma once

#include <cstddef>

namespace crossa::compiler::source {

// Identifies one one-based line and column inside a loaded source file.
// Accessors provide stable locations for AST and semantic diagnostics.
class SourceLocation final {
public:
    // Creates a source location from its one-based line and column.
    SourceLocation(std::size_t line, std::size_t column) noexcept;

    // Returns the one-based source line.
    [[nodiscard]] std::size_t getLine() const noexcept;

    // Returns the one-based source column.
    [[nodiscard]] std::size_t getColumn() const noexcept;

private:
    std::size_t line_;
    std::size_t column_;
};

}
