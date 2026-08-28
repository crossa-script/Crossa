#include "crossa/utils/PrintUtils.h"

#include <iostream>

using namespace std;

namespace crossa::utils {

void PrintUtils::println(const string& message) {
    std::cout << message << '\n';
}

void PrintUtils::println(const string& message, string_view color) {
    std::cout << color << message << ResetColor << '\n';
}

}
