#include "crossa/packaging/android/AndroidBuildRequirements.h"

#include <cstdlib>
#include <filesystem>
#include <string_view>

using namespace std;

namespace crossa::packaging::android {

    // Compares and selects Android SDK tool versions using semantic segments.
    class AndroidToolVersionSelector final {
    public:
        // Returns the highest installed NDK version that satisfies the minimum.
        [[nodiscard]] static string selectInstalledNdkVersion(
            const string& minimumVersion
        ) {
            const char* androidHome = getenv("ANDROID_HOME");
            if (androidHome == nullptr || string_view(androidHome).empty()) {
                return minimumVersion;
            }
            error_code error;
            const filesystem::path ndkDirectory =
                filesystem::path(androidHome) / "ndk";
            if (!filesystem::is_directory(ndkDirectory, error) || error) {
                return minimumVersion;
            }

            string selectedVersion;
            for (const filesystem::directory_entry& entry :
                 filesystem::directory_iterator(ndkDirectory, error)) {
                if (error) {
                    break;
                }
                if (!entry.is_directory(error) || error) {
                    error.clear();
                    continue;
                }
                const string version = entry.path().filename().string();
                if (compareVersions(version, minimumVersion) < 0) {
                    continue;
                }
                if (selectedVersion.empty() ||
                    compareVersions(version, selectedVersion) > 0) {
                    selectedVersion = version;
                }
            }
            return selectedVersion.empty() ? minimumVersion : selectedVersion;
        }

    private:
        // Compares dotted numeric version strings.
        [[nodiscard]] static int compareVersions(
            string_view left,
            string_view right
        ) noexcept {
            size_t leftStart = 0;
            size_t rightStart = 0;
            while (leftStart < left.size() || rightStart < right.size()) {
                const size_t leftEnd = left.find('.', leftStart);
                const size_t rightEnd = right.find('.', rightStart);
                const int leftValue = parseSegment(
                    left.substr(
                        leftStart,
                        leftEnd == string_view::npos
                            ? string_view::npos
                            : leftEnd - leftStart
                    )
                );
                const int rightValue = parseSegment(
                    right.substr(
                        rightStart,
                        rightEnd == string_view::npos
                            ? string_view::npos
                            : rightEnd - rightStart
                    )
                );
                if (leftValue != rightValue) {
                    return leftValue < rightValue ? -1 : 1;
                }
                if (leftEnd == string_view::npos &&
                    rightEnd == string_view::npos) {
                    break;
                }
                leftStart = leftEnd == string_view::npos
                    ? left.size()
                    : leftEnd + 1;
                rightStart = rightEnd == string_view::npos
                    ? right.size()
                    : rightEnd + 1;
            }
            return 0;
        }

        // Parses one numeric semantic-version segment.
        [[nodiscard]] static int parseSegment(string_view value) noexcept {
            int result = 0;
            for (const char character : value) {
                if (character < '0' || character > '9') {
                    break;
                }
                result = (result * 10) + static_cast<int>(character - '0');
            }
            return result;
        }
    };

    // Returns the Android SDK platform API required by generated AAR builds.
    int AndroidBuildRequirements::compileSdkVersion() noexcept {
        return 35;
    }

    // Returns the minimum side-by-side Android NDK version required by generated AAR builds.
    string AndroidBuildRequirements::ndkVersion() {
        return "28.1.13356709";
    }

    // Returns the installed Android NDK version selected for generated Gradle builds.
    string AndroidBuildRequirements::recommendedNdkVersion() {
        return AndroidToolVersionSelector::selectInstalledNdkVersion(
            ndkVersion()
        );
    }

    // Returns the minimum CMake version declared by generated native build files.
    string AndroidBuildRequirements::cmakeMinimumVersion() {
        return "3.22.1";
    }

    // Returns the Java toolchain major version required by generated Gradle builds.
    int AndroidBuildRequirements::javaToolchainVersion() noexcept {
        return 17;
    }

    // Returns the Android Gradle Plugin version used by generated AAR builds.
    string AndroidBuildRequirements::androidGradlePluginVersion() {
        return "8.5.1";
    }

    // Returns the Kotlin Android plugin version used by generated AAR builds.
    string AndroidBuildRequirements::kotlinAndroidPluginVersion() {
        return "2.0.21";
    }

}
