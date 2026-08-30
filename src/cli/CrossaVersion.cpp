#include "crossa/cli/CrossaVersion.h"

#include <string>

using namespace std;

namespace crossa::cli {

    // Returns the current Crossa CLI version.
    string CrossaVersion::current() {
        return "0.1.0";
    }

}
