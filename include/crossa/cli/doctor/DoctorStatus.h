#pragma once

namespace crossa::cli::doctor {

// Represents one doctor check outcome for reporting and exit-code decisions.
enum class DoctorStatus {
    Passed,
    Warning,
    Failed
};

}
