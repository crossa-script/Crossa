#include "crossa/cli/TerminalCapabilities.h"

#include <cstdio>

#if defined(_WIN32)
#include <io.h>
#else
#include <unistd.h>
#endif

namespace crossa::cli {

// Returns true when standard input is attached to a terminal.
bool TerminalCapabilities::isInteractiveInput() noexcept {
#if defined(_WIN32)
    return _isatty(_fileno(stdin)) != 0;
#else
    return isatty(fileno(stdin)) != 0;
#endif
}

// Returns true when standard output is attached to a terminal.
bool TerminalCapabilities::isInteractiveOutput() noexcept {
#if defined(_WIN32)
    return _isatty(_fileno(stdout)) != 0;
#else
    return isatty(fileno(stdout)) != 0;
#endif
}

}
