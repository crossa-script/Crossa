#include "crossa/compiler/source/SourceLocation.h"

using namespace std;

namespace crossa::compiler::source {

    // Creates a source location from its one-based line and column.
    SourceLocation::SourceLocation(size_t line, size_t column) noexcept
        : line_(line), column_(column) {}

    // Returns the one-based source line.
    size_t SourceLocation::getLine() const noexcept {
        return line_;
    }

    // Returns the one-based source column.
    size_t SourceLocation::getColumn() const noexcept {
        return column_;
    }

}
