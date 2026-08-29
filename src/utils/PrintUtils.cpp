#include "crossa/utils/PrintUtils.h"

#include <iostream>

using namespace std;

namespace crossa::utils {

    // Prints a message followed by a line terminator.
    void PrintUtils::println(const string& message) {
        lock_guard lock(OutputMutex);
        std::cout << message << '\n';
    }

    // Prints a colored message followed by a line terminator.
    void PrintUtils::println(const string& message, string_view color) {
        lock_guard lock(OutputMutex);
        std::cout << color << message << ResetColor << '\n';
    }

}
