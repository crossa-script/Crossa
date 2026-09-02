#include "crossa/compiler/generators/swift/SwiftGeneratedSource.h"

#include <utility>

using namespace std;

namespace crossa::compiler::generators::swift {

    // Creates one generated Swift source artifact.
    SwiftGeneratedSource::SwiftGeneratedSource(string fileName, string content)
        : fileName_(std::move(fileName)), content_(std::move(content)) {}

    // Returns the deterministic path relative to the generated Swift root.
    const string& SwiftGeneratedSource::getFileName() const noexcept {
        return fileName_;
    }

    // Returns the complete canonical Swift source text.
    const string& SwiftGeneratedSource::getContent() const noexcept {
        return content_;
    }

}
