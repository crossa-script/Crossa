#pragma once

#include "crossa/bindings/shared-abi/CrossaAbi.h"

#ifdef __cplusplus
extern "C" {
#endif

// Creates the runtime reconstructed from the generated Crossa program.
CrossaStatus crossaIosCreateGeneratedRuntime(CrossaRuntimeHandle* runtime);

#ifdef __cplusplus
}
#endif
