#pragma once

#include <string>

#include "crossa/cli/doctor/DoctorStatus.h"

namespace crossa::cli::doctor {

// Stores one structured doctor line before presentation formats it.
// group, name, value, and remediation keep checks independent from stdout.
struct DoctorResult final {
    DoctorStatus status;
    std::string group;
    std::string name;
    std::string value;
    std::string remediation;
};

}
