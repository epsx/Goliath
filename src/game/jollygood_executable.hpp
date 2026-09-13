// jollygood_executable.hpp — canonical resolver for the JGRF executable
// configured in Goliath. All UI/probe/launch code should use this helper so
// Windows extension handling stays identical everywhere.
#pragma once

#include <filesystem>

namespace goliath {

// Resolves the configured JGRF executable path. The configured path is already
// based on Goliath's [Paths] resolution; on Windows, also try the normal .exe
// suffix used by the shipped/default configuration.
std::filesystem::path resolve_jollygood_executable(std::filesystem::path configured);

} // namespace goliath
