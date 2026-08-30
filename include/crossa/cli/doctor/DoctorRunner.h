#pragma once

#include <string>
#include <vector>

#include "crossa/cli/doctor/DoctorResult.h"

namespace crossa::cli::doctor {

// Owns the complete result set produced by a doctor run.
// hasFailures() determines the process exit status.
class DoctorReport final {
public:
    // Creates a report from already collected doctor results.
    explicit DoctorReport(std::vector<DoctorResult> results);

    // Returns every result in display order.
    [[nodiscard]] const std::vector<DoctorResult>& getResults() const noexcept;

    // Returns true when at least one required check failed.
    [[nodiscard]] bool hasFailures() const noexcept;

private:
    std::vector<DoctorResult> results_;
};

// Coordinates all doctor checks in deterministic display order.
// run() collects structured results without printing directly.
class DoctorRunner final {
public:
    // Runs all Crossa, host, Android, and storage checks.
    [[nodiscard]] DoctorReport run(const std::string& executableArgument) const;
};

}
