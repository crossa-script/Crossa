#include "crossa/bindings/ios/CrossaIosRuntimeBridge.h"

#include <utility>

#include "CrossaGeneratedProgram.h"
#include "crossa/bindings/shared-abi/CrossaAbiRuntimeFactory.h"
#include "crossa/utils/Log.h"

using namespace std;

// Creates the generated iOS runtime through the stable ABI factory.
extern "C" CrossaStatus crossaIosCreateGeneratedRuntime(
    CrossaRuntimeHandle* runtime
) {
    if (runtime == nullptr) return CrossaStatusInvalidArgument;
    try {
        static const crossa::utils::Log Log(crossa::utils::Log::Level::Error);
        crossa::compiler::ir::Program program =
            crossa::generated::CrossaGeneratedProgram::create();
        return crossa::bindings::sharedabi::CrossaAbiRuntimeFactory::create(
            std::move(program),
            nullptr,
            Log,
            runtime
        );
    } catch (...) {
        *runtime = 0;
        return CrossaStatusInternalError;
    }
}
