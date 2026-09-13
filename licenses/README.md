# License materials

This directory contains the license and attribution materials for third-party
source and Windows runtime components used or redistributed with Goliath.

- `source/` covers dependencies vendored in the Goliath source tree.
- `upstream/` covers JG, JGRF, Geolith, libchdr, and their bundled material.
- `msys2/` contains the license files shipped by the exact MSYS2 UCRT64
  packages used for the audited Windows portable build.
- `msys2/qt6-base/qtbase-6.11.1.spdx` is Qt Base's installed SPDX SBOM.
- `SHA256SUMS` records the exact bytes of every other file in this directory.

These files do not place all components under one license. Each component
retains the license identified in `../THIRD-PARTY-NOTICES.md` and in its own
license material. Goliath's original project material is covered separately
by `../LICENSE` under GPL-3.0-or-later.
