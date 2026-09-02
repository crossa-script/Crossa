#include "crossa/cli/ProjectSourceDiscovery.h"

#include <algorithm>
#include <stdexcept>
#include <system_error>

using namespace std;

namespace crossa::cli {

// Returns sorted non-configuration .cra files under a project directory.
vector<filesystem::path> ProjectSourceDiscovery::find(
    const filesystem::path& projectDirectory
) {
    vector<filesystem::path> sourcePaths;
    error_code error;
    filesystem::recursive_directory_iterator iterator(
        projectDirectory,
        filesystem::directory_options::skip_permission_denied,
        error
    );
    if (error) {
        throw runtime_error(
            "Unable to inspect Crossa project directory: " +
            projectDirectory.string()
        );
    }

    const filesystem::recursive_directory_iterator end;
    while (iterator != end) {
        if (iterator->is_regular_file(error) &&
            iterator->path().extension() == ".cra" &&
            iterator->path().filename() != "config.cra") {
            sourcePaths.push_back(iterator->path());
        }
        iterator.increment(error);
        if (error) {
            throw runtime_error(
                "Unable to inspect Crossa project directory: " +
                projectDirectory.string()
            );
        }
    }

    sort(
        sourcePaths.begin(),
        sourcePaths.end(),
        [&projectDirectory](
            const filesystem::path& left,
            const filesystem::path& right
        ) {
            return left.lexically_relative(projectDirectory).generic_string() <
                right.lexically_relative(projectDirectory).generic_string();
        }
    );
    return sourcePaths;
}

}
