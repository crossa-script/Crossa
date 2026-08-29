#include "crossa/compiler/generators/kotlin/KotlinGeneratedSource.h"

#include <utility>

using namespace std;

namespace crossa::compiler::generators::kotlin {

    // Creates one generated Kotlin source artifact.
    KotlinGeneratedSource::KotlinGeneratedSource(
        string fileName,
        string content
    )
        : fileName_(std::move(fileName)), content_(std::move(content)) {}

    // Returns the deterministic Kotlin file name.
    const string& KotlinGeneratedSource::getFileName() const noexcept {
        return fileName_;
    }

    // Returns the complete canonical Kotlin source content.
    const string& KotlinGeneratedSource::getContent() const noexcept {
        return content_;
    }

}
