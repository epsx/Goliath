#pragma once

#include <QString>

namespace goliath {

inline QString universe_bios_help_text() {
    return QStringLiteral(
        "System Type\n"
        "Used for cartridge (.neo) games. AES and MVS select the original "
        "console or arcade BIOS; Universe BIOS loads the cartridge Universe BIOS.\n\n"
        "CD System\n"
        "Used only for Neo Geo CD games. It independently selects Front Loader, "
        "Top Loader, CDZ, or CD Universe BIOS. It does not depend on System Type.\n\n"
        "Universe BIOS HW\n"
        "Used only when System Type is Universe BIOS. It selects whether cartridge "
        "hardware starts as AES (console) or MVS (arcade). It does not affect CD "
        "Universe BIOS.\n\n"
        "Region\n"
        "Selects US, JP, AS (Asia), or EU. In original AES/MVS modes it selects the "
        "matching regional BIOS. Universe BIOS can also change region and AES/MVS "
        "mode from its own menu.\n\n"
        "Changes take effect on the next game launch.");
}

} // namespace goliath
