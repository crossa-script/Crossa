#include "crossa/compiler/source/SourceLoader.h"

#include <fstream>
#include <iterator>
#include <stdexcept>
#include <utility>

using namespace std;

namespace crossa::compiler::source {

    // Loads a .cra file and returns its path and complete content.
    SourceFile SourceLoader::load(
        const filesystem::path& path,
        const utils::Log& log
    ) {
        log.debug("Source loading started: " + path.string());
        log.debug("Validating source extension");

        if (!hasSupportedExtension(path)) {
            throw invalid_argument("Execution extension not supported: expected .cra");
        }

        log.debug("Source extension accepted");
        log.debug("Opening source file");

        ifstream input(path, ios::binary);
        if (!input.is_open()) {
            throw runtime_error("Unable to read Crossa source file: " + path.string());
        }

        string content{
            istreambuf_iterator<char>(input),
            istreambuf_iterator<char>()
        };

        log.debug("Source content loaded: " + to_string(content.size()) + " bytes");

        return SourceFile(path, std::move(content));
    }

    // Checks whether the path uses the supported .cra extension.
    bool SourceLoader::hasSupportedExtension(
        const filesystem::path& path
    ) noexcept {
        return path.extension() == ".cra";
    }

}
