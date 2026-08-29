#include "crossa/compiler/source/SourceLocation.h"

#include <utility>

using namespace std;

namespace crossa::compiler::source {

    // Creates a source location from its one-based line and column.
    SourceLocation::SourceLocation(size_t line, size_t column) noexcept
        : sourcePath_(nullptr), line_(line), column_(column) {}

    // Creates a source location with its originating file path.
    SourceLocation::SourceLocation(
        string sourcePath,
        size_t line,
        size_t column
    )
        : sourcePath_(make_shared<const string>(std::move(sourcePath))),
          line_(line),
          column_(column) {}

    // Creates a source location sharing an interned source path.
    SourceLocation::SourceLocation(
        shared_ptr<const string> sourcePath,
        size_t line,
        size_t column
    ) noexcept
        : sourcePath_(std::move(sourcePath)),
          line_(line),
          column_(column) {}

    // Returns the one-based source line.
    size_t SourceLocation::getLine() const noexcept {
        return line_;
    }

    // Returns the one-based source column.
    size_t SourceLocation::getColumn() const noexcept {
        return column_;
    }

    // Returns the source path or an empty string when it is unavailable.
    string_view SourceLocation::getSourcePath() const noexcept {
        return sourcePath_ == nullptr ? string_view() : string_view(*sourcePath_);
    }

}
