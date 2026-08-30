#pragma once

#include <string>

namespace crossa::cli {

// Exposes the current Crossa CLI version for commands and diagnostics.
// current() returns the semantic version without a leading tag prefix.
class CrossaVersion final {
public:
    // Returns the current Crossa CLI version.
    [[nodiscard]] static std::string current();
};

}
