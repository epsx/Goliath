#pragma once

namespace goliath {

struct AppPaths;

// Prepare the BIOS path layout expected by the configured JGRF build without
// modifying the user's configured BIOS directory. Dynamic-core launches use
// XDG_DATA_HOME/jollygood/bios; static-core launches use a BIOS directory next
// to jollygood. Falls back to copying into an existing plain
// directory when linking is unavailable, but never recursively removes a
// plain directory or replaces a non-directory object at the BIOS destination.
void ensure_jollygood_bios(const AppPaths& paths);

} // namespace goliath
