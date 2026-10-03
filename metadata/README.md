# Bundled Neo Geo reference metadata

Goliath ships these non-executable reference files for game identification and
integrity verification. They contain names, identifiers, sizes, and checksums;
they do not contain game ROMs, disc images, BIOS files, or encryption keys.

| File | Purpose | Upstream snapshot | SHA-256 |
| --- | --- | --- | --- |
| `geolith.xml` | Maps supported TerraOnion `.neo` filenames to Geolith CRC-32 values | Generated from [`rename-neo.sh`](https://gitlab.com/jgemu/geolith/-/blob/d3104014927b5b72fc48e93c3d7e033754a61956/rename-neo.sh), tag `0.4.2`, commit `d3104014927b5b72fc48e93c3d7e033754a61956`, source SHA-256 `0f9ec3062433c423b1440806a7525ee6ec7dbe9a31e7d90d61db8c2688a86276` | `5e86fb1b84de76965d8898ceb17f4791fbfe25e4d2fe3aaedfb8c4ef359dfecc` |
| `neogeo.xml` | MVS/AES software metadata and ROM-region checksums | [MAME `hash/neogeo.xml`](https://github.com/mamedev/mame/blob/master/hash/neogeo.xml), captured 2026-10-01 | `7a5336c9d4f337f2cc41a1171ac2a5fa9464662e59b2c1d1133bdbc27fe42402` |
| `neocd.xml` | Neo Geo CD metadata and MAME CHD disk SHA-1 values | [MAME `hash/neocd.xml`](https://github.com/mamedev/mame/blob/master/hash/neocd.xml), captured 2026-10-01 | `b65c1c6b2d6bb545d7d076917f8b1f4426f478bab834a08bf523c8c4e212b212` |
| `softwarelist.dtd` | XML schema used by the two MAME software lists | [MAME `hash/softwarelist.dtd`](https://github.com/mamedev/mame/blob/master/hash/softwarelist.dtd), captured 2026-10-01 | `3b14fa382113bc1c259b2a119346b0c7b4777ebdd52e6610bdc293008af4b549` |

## Licenses and attribution

- `geolith.xml` is derived from Geolith's `rename-neo.sh`. Its complete
  Copyright 2024 orbea notice, redistribution condition, and disclaimer are
  embedded at the beginning of the XML and must remain intact.
- `neogeo.xml` and `neocd.xml` declare `CC0-1.0` in their upstream headers.
  The complete CC0 text is retained in
  `../licenses/upstream/jgrf/CC0-1.0.txt`.
- `softwarelist.dtd` has no file-specific license header. It is conservatively
  distributed under the MAME project's GPL-2.0-or-later terms; the complete
  text is retained in
  `../licenses/msys2/qt6-base/GPL-2.0-or-later.txt`.
- MAME is a registered trademark of Gregory Ember. Goliath is independent and
  uses the name only to identify provenance and compatibility.

The detailed third-party disclosure is in `../THIRD-PARTY-NOTICES.md`.

## Updating

Do not silently replace these files. For every update:

1. record the upstream tag or commit when available and the capture date;
2. preserve every upstream license header;
3. regenerate the SHA-256 values in this document;
4. validate a complete rescan and the metadata parser tests;
5. update `THIRD-PARTY-NOTICES.md` if provenance or licensing changes.

Redump DAT files, `command.dat`, history databases, artwork, and supplemental
INI catalogs are not bundled. Users may place copies they are entitled to use
in the configured metadata directory.
