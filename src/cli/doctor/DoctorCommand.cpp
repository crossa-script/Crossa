#include "crossa/cli/doctor/DoctorCommand.h"

#include <set>
#include <string>

#include "crossa/utils/PrintUtils.h"

using namespace std;

namespace crossa::cli::doctor {

    // Formats doctor status as the public CLI symbol.
    class DoctorStatusFormatter final {
    public:
        // Returns the symbol associated with one doctor status.
        [[nodiscard]] static string symbol(DoctorStatus status) {
            switch (status) {
                case DoctorStatus::Passed:
                    return "✓";
                case DoctorStatus::Warning:
                    return "!";
                case DoctorStatus::Failed:
                    return "✗";
            }
            return "!";
        }
    };

    // Formats one doctor result row for deterministic console output.
    class DoctorLineFormatter final {
    public:
        // Returns a padded result line with symbol, name, and value.
        [[nodiscard]] static string format(const DoctorResult& result) {
            string line = DoctorStatusFormatter::symbol(result.status) + " ";
            line += result.name;
            if (result.name.size() < NameWidth) {
                line += string(NameWidth - result.name.size(), ' ');
            } else {
                line += " ";
            }
            line += result.value;
            return line;
        }

    private:
        inline static constexpr size_t NameWidth = 24;
    };

    // Executes crossa doctor for the current executable and returns an exit code.
    int DoctorCommand::run(const string& executableArgument) {
        const DoctorRunner runner;
        const DoctorReport report = runner.run(executableArgument);
        printReport(report);
        return report.hasFailures() ? 1 : 0;
    }

    // Prints the complete doctor report in grouped CLI format.
    void DoctorCommand::printReport(const DoctorReport& report) {
        utils::PrintUtils::println("Crossa Doctor");

        string currentGroup;
        for (const DoctorResult& result : report.getResults()) {
            if (result.group != currentGroup) {
                utils::PrintUtils::println("");
                utils::PrintUtils::println(result.group);
                currentGroup = result.group;
            }
            utils::PrintUtils::println(DoctorLineFormatter::format(result));
        }

        utils::PrintUtils::println("");
        if (report.hasFailures()) {
            utils::PrintUtils::println(
                "Crossa has missing required build prerequisites."
            );
            printRemediation(report);
        } else {
            utils::PrintUtils::println(
                "Crossa is ready to build supported artifacts on this host."
            );
        }
    }

    // Prints actionable remediation lines for failed results.
    void DoctorCommand::printRemediation(const DoctorReport& report) {
        set<string> remediations;
        for (const DoctorResult& result : report.getResults()) {
            if (result.status == DoctorStatus::Failed &&
                !result.remediation.empty()) {
                remediations.insert(result.remediation);
            }
        }
        if (remediations.empty()) {
            return;
        }

        utils::PrintUtils::println("");
        utils::PrintUtils::println("Fix:");
        for (const string& remediation : remediations) {
            utils::PrintUtils::println("- " + remediation);
        }
    }

}
