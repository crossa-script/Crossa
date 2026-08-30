#pragma once

#include <string>

#include "crossa/cli/doctor/DoctorRunner.h"

namespace crossa::cli::doctor {

// Implements the public crossa doctor command.
// run() executes checks, renders the report, and returns the process status.
class DoctorCommand final {
public:
    // Executes crossa doctor for the current executable and returns an exit code.
    [[nodiscard]] static int run(const std::string& executableArgument);

private:
    // Prints the complete doctor report in grouped CLI format.
    static void printReport(const DoctorReport& report);

    // Prints actionable remediation lines for failed results.
    static void printRemediation(const DoctorReport& report);
};

}
