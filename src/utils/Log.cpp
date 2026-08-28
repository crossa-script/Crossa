#include "crossa/utils/Log.h"

#include "crossa/utils/PrintUtils.h"

using namespace std;

namespace crossa::utils {

void Log::info(const string& message) {
    PrintUtils::println("[INFO] " + message);
}

void Log::warning(const string& message) {
    PrintUtils::println("[WARNING] " + message, WarningColor);
}

void Log::error(const string& message) {
    PrintUtils::println("[ERROR] " + message, ErrorColor);
}

}
