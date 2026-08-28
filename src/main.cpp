#include "crossa/cli/CrossaApplication.h"

using namespace crossa::cli;

// Delegates command-line execution to the Crossa application workflow.
int main(int argc, char* argv[]) {
    return CrossaApplication::run(argc, argv);
}
