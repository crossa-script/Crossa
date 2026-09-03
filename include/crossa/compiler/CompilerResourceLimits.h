#pragma once

#include <cstddef>

namespace crossa::compiler {

class CompilerResourceLimits final {
public:
    static constexpr std::size_t MaximumSourceBytes = 16U * 1024U * 1024U;
    static constexpr std::size_t MaximumProjectSourceFiles = 10000U;
    static constexpr std::size_t MaximumProjectSourceBytes = 256U * 1024U * 1024U;
    static constexpr std::size_t MaximumImportedModules = 1024U;
    static constexpr std::size_t MaximumImportDepth = 256U;
    static constexpr std::size_t MaximumParseRecursionDepth = 512U;
};

}
