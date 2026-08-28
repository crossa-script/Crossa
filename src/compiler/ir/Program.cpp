#include "crossa/compiler/ir/Program.h"

#include <utility>

using namespace std;

namespace crossa::compiler::ir {

    // Creates an IR program from a loaded source file.
    Program::Program(source::SourceFile sourceFile)
        : sourceFile_(std::move(sourceFile)) {}

    // Returns the source file stored by this IR program.
    const source::SourceFile& Program::getSourceFile() const noexcept {
        return sourceFile_;
    }

}
