#pragma once

#include <cstddef>
#include <memory>
#include <string>
#include <string_view>

namespace crossa::compiler::source {

// Identifies one-based coordinates and origin inside a loaded source file.
// Accessors provide stable locations for AST and semantic diagnostics.
class SourceLocation final {
public:
    // Creates a source location from its one-based line and column.
    SourceLocation(std::size_t line, std::size_t column) noexcept;

    // Creates a source location with its originating file path.
    SourceLocation(
        std::string sourcePath,
        std::size_t line,
        std::size_t column
    );

    // Creates a source location sharing an interned source path.
    SourceLocation(
        std::shared_ptr<const std::string> sourcePath,
        std::size_t line,
        std::size_t column
    ) noexcept;

    // Returns the one-based source line.
    [[nodiscard]] std::size_t getLine() const noexcept;

    // Returns the one-based source column.
    [[nodiscard]] std::size_t getColumn() const noexcept;

    // Returns the source path or an empty string when it is unavailable.
    [[nodiscard]] std::string_view getSourcePath() const noexcept;

private:
    std::shared_ptr<const std::string> sourcePath_;
    std::size_t line_;
    std::size_t column_;
};

}
