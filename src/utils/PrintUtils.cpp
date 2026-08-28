#include "crossa/utils/PrintUtils.h"

#include <iostream>

using namespace std;

namespace crossa::utils {

    // Prints a message followed by a line terminator.
    void PrintUtils::println(const string& message) {
        std::cout << message << '\n';
    }

    // Prints a colored message followed by a line terminator.
    void PrintUtils::println(const string& message, string_view color) {
        std::cout << color << message << ResetColor << '\n';
    }

}
