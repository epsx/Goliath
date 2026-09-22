# License materials

This directory contains the license and attribution materials for third-party
source and Windows runtime components used or redistributed with Goliath.

- `source/` covers dependencies vendored in the Goliath source tree.
- `upstream/` covers JG, JGRF, Geolith, libchdr, and their bundled material.
- `msys2/` contains the license files for the audited Windows portable build.
  CI refreshes the license directories of deployed DLL packages from its
  installed MSYS2 packages and regenerates this manifest before upload.
- `msys2/libffi/` and `msys2/libjpeg-turbo/` cover transitive DLLs collected
  from the deployed Qt runtime.
- `msys2/qt6-base/` contains the full installed license directory from the
  exact Qt Base 6.11.2-2 package. This release of that package has no installed
  SPDX file.
- `SHA256SUMS` records the exact bytes of every other file in this directory.

These files do not place all components under one license. Each component
retains the license identified in `../THIRD-PARTY-NOTICES.md` and in its own
license material. Goliath's original project material is covered separately
by `../LICENSE` under GPL-3.0-or-later.
