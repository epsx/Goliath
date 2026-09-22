# Goliath release compliance checklist

This is the release checklist for the audited Goliath Windows portable build.
It records the selected licensing path and the files that must accompany a
public binary. It is an operational record, not legal advice.

## Release boundary

Goliath's original project material is licensed under GPL-3.0-or-later.
Third-party source, executables, libraries, plugins, shaders, and artwork keep
their own licenses as listed in `THIRD-PARTY-NOTICES.md`.

A public portable archive must contain, at minimum:

- `LICENSE`;
- `THIRD-PARTY-NOTICES.md`;
- the complete `licenses/` directory generated from the audited components;
- the Goliath executable and only the runtime files actually required by the
  release.

The build copies the three legal items beside `goliath-qt` automatically. A
release staging procedure must not remove them.

## Corresponding source

Publish the exact Goliath source used for each binary release from the same
release location and at no additional charge. The source must include the
build definitions, vendored source dependencies, documentation, and any local
changes required to reproduce the Goliath executable.

The Windows package redistributes dynamically linked Qt libraries under
LGPL-3.0-only. For the exact Qt binaries in a release:

1. keep the Qt notice, LGPL/GPL texts, and the package's complete installed
   Qt attribution files from `licenses/msys2/qt6-base/`; the audited MSYS2
   Qt Base 6.11.2-2 package does not install an SPDX SBOM;
2. permit replacement of the Qt DLLs and reverse engineering for debugging
   such replacements;
3. publish the complete corresponding Qt Base source used by the MSYS2 build
   from the same release location at no additional charge, including the
   applicable MSYS2 build recipe and patches;
4. identify the source archive unambiguously from the release notes.

Do not rely on an ordinary upstream home-page link as the only source offer.
The archived source must correspond to the binaries actually distributed.

The documented JGRF and libepoxy compatibility changes remain under their
permissive upstream licenses. Preserve the patch files, exact upstream
revisions, recipes, and reconstruction record described in
`WINDOWS_OPENGL_ES_WGL_RUNTIME_FIX.md`.

## Content boundary

Do not place any of the following in the public source or binary release:

- game ROMs or optical-disc images;
- BIOS or firmware files;
- encryption keys;
- proprietary game artwork;
- external MAME, Redump, or supplemental metadata catalogs.

Goliath does not provide links for obtaining that content. Runtime directories
for it are user-managed and remain outside the release payload.

## Final release checks

Before publication:

1. generate the payload from a clean staging directory;
2. verify that `LICENSE`, `THIRD-PARTY-NOTICES.md`, and every file under
   `licenses/` are present and pass `licenses/SHA256SUMS`;
3. inspect imports of the actual EXE and DLL files and compare them with the
   third-party inventory;
4. verify the Goliath and Qt corresponding-source archives;
5. confirm the package contains no ROM, disc-image, BIOS, firmware, metadata,
   save-data, log, configuration, or developer-machine residue;
6. test the archive on a clean Windows machine without MSYS2;
7. publish hashes for the binary and corresponding-source archives.

If any dependency or runtime file changes, repeat the dependency and license
audit before publishing the new package.
