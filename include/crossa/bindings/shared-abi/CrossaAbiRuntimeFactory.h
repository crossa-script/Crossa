#pragma once

#include "crossa/bindings/shared-abi/CrossaAbi.h"
#include "crossa/compiler/ir/Program.h"
#include "crossa/utils/Log.h"

namespace crossa::bindings::sharedabi {

// Creates ABI-addressable NativeRuntime instances from generated IR programs.
class CrossaAbiRuntimeFactory final {
public:
    static CrossaStatus create(
        compiler::ir::Program program,
        const compiler::ir::Program* configurationProgram,
        const utils::Log& log,
        CrossaRuntimeHandle* runtime
    );
};

}
