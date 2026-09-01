#include "crossa/utils/PrintUtils.h"

#if defined(__ANDROID__)
#include <android/log.h>
#else
#include <iostream>
#endif

using namespace std;

namespace crossa::utils {

    // Prints a message followed by a line terminator.
    void PrintUtils::println(const string& message) {
        lock_guard lock(OutputMutex);
#if defined(__ANDROID__)
        __android_log_print(ANDROID_LOG_INFO, "Crossa", "%s", message.c_str());
#else
        std::cout << message << '\n';
#endif
    }

    // Prints a colored message followed by a line terminator.
    void PrintUtils::println(const string& message, string_view color) {
        lock_guard lock(OutputMutex);
#if defined(__ANDROID__)
        (void)color;
        __android_log_print(ANDROID_LOG_INFO, "Crossa", "%s", message.c_str());
#else
        std::cout << color << message << ResetColor << '\n';
#endif
    }

}
