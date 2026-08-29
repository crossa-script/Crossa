#include "crossa/compiler/generators/kotlin/KotlinSourceWriter.h"

#include <stdexcept>

using namespace std;

namespace crossa::compiler::generators::kotlin {

    // Creates an empty writer with the requested initial output capacity.
    KotlinSourceWriter::KotlinSourceWriter(size_t initialCapacity)
        : indentationLevel_(0) {
        source_.reserve(initialCapacity);
    }

    // Writes one complete indented line.
    void KotlinSourceWriter::writeLine(const string& line) {
        if (line.empty()) {
            source_.push_back('\n');
            return;
        }
        source_.append(indentationLevel_ * 4, ' ');
        source_.append(line);
        source_.push_back('\n');
    }

    // Begins one Kotlin block after writing its header.
    void KotlinSourceWriter::beginBlock(const string& header) {
        writeLine(header + " {");
        indent();
    }

    // Ends the current Kotlin block.
    void KotlinSourceWriter::endBlock() {
        dedent();
        writeLine("}");
    }

    // Increases indentation for nested Kotlin content.
    void KotlinSourceWriter::indent() {
        ++indentationLevel_;
    }

    // Decreases indentation after nested Kotlin content.
    void KotlinSourceWriter::dedent() {
        if (indentationLevel_ == 0) {
            throw runtime_error(
                "Kotlin generation invariant failed: source writer indentation underflow."
            );
        }
        --indentationLevel_;
    }

    // Returns the assembled source with exactly one trailing newline.
    const string& KotlinSourceWriter::getSource() const noexcept {
        return source_;
    }

}
