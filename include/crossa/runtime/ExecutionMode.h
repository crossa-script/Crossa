#pragma once

namespace crossa::runtime {

// Selects whether the native runtime executes a script or validates tests.
enum class ExecutionMode {
    Run,
    Test
};

}
