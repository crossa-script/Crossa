#include "crossa/cli/doctor/DoctorRunner.h"

#include <iterator>
#include <utility>

#include "crossa/cli/doctor/DoctorChecks.h"

using namespace std;

namespace crossa::cli::doctor {

    // Creates a report from already collected doctor results.
    DoctorReport::DoctorReport(vector<DoctorResult> results) :
        results_(std::move(results)) {}

    // Returns every result in display order.
    const vector<DoctorResult>& DoctorReport::getResults() const noexcept {
        return results_;
    }

    // Returns true when at least one required check failed.
    bool DoctorReport::hasFailures() const noexcept {
        for (const DoctorResult& result : results_) {
            if (result.status == DoctorStatus::Failed) {
                return true;
            }
        }
        return false;
    }

    // Runs all Crossa, host, Android, and storage checks.
    DoctorReport DoctorRunner::run(const string& executableArgument) const {
        vector<DoctorResult> results;
        const auto append = [&results](vector<DoctorResult> nextResults) {
            results.insert(
                results.end(),
                make_move_iterator(nextResults.begin()),
                make_move_iterator(nextResults.end())
            );
        };

        append(CrossaInstallationCheck(executableArgument).run());
        append(HostCheck().run());
        append(AndroidSdkCheck().run());
        append(AndroidNdkCheck().run());
        append(CMakeCheck().run());
        append(NinjaCheck().run());
        append(JavaCheck().run());
        append(StorageCheck().run());
        return DoctorReport(std::move(results));
    }

}
