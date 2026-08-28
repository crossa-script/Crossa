#pragma once

#include "crossa/compiler/source/SourceFile.h"

namespace crossa::compiler::ir {

// Owns the initial IR representation of one loaded Crossa source unit.
// getSourceFile() exposes the source input for later compiler stages.
class Program final {
public:
    // Creates an IR program from a loaded source file.
    explicit Program(source::SourceFile sourceFile);

    // Returns the source file stored by this IR program.
    [[nodiscard]] const source::SourceFile& getSourceFile() const noexcept;

private:
    source::SourceFile sourceFile_;
};

}
