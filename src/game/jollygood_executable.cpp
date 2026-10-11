#include "game/jollygood_executable.hpp"

#include "common/qt_path.hpp"

#include <algorithm>
#include <cctype>
#include <string>

namespace fs = std::filesystem;

namespace goliath {

fs::path resolve_jollygood_executable(fs::path configured) {
    if (configured.empty()) return {};

    std::error_code ec;
    if (fs::is_regular_file(configured, ec)) return configured;

#if defined(_WIN32)
    std::string extension = path_to_utf8(configured.extension());
    std::transform(extension.begin(), extension.end(), extension.begin(),
                   [](unsigned char ch) {
                       return static_cast<char>(std::tolower(ch));
                   });

    if (extension != ".exe") {
        configured += ".exe";
        ec.clear();
        if (fs::is_regular_file(configured, ec)) return configured;
    }
#endif

    return {};
}

} // namespace goliath
