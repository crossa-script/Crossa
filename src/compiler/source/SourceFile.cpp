#include "crossa/compiler/source/SourceFile.h"

#include <utility>

using namespace std;

namespace crossa::compiler::source {

    // Creates a source file value with its path and content.
    SourceFile::SourceFile(filesystem::path path, string content)
        : path_(std::move(path)), content_(std::move(content)) {}

    // Returns the source file path.
    const filesystem::path& SourceFile::getPath() const noexcept {
        return path_;
    }

    // Returns the complete source file content.
    const string& SourceFile::getContent() const noexcept {
        return content_;
    }

}
