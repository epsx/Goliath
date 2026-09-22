# Goliath third-party notices

This document identifies third-party software and assets used by the Goliath
source tree or redistributed in the audited Windows portable build. Each
component remains under its own license. Nothing in Goliath's GPL license
relicenses these components.

The license texts from the audited source and installed runtime packages are
under `licenses/`. The portable package must retain this document, `LICENSE`,
and the complete `licenses/` directory.

## Source dependencies

| Component | Version | Use | License material |
| --- | --- | --- | --- |
| nlohmann/json | 3.11.3 | Vendored JSON implementation | MIT; `licenses/source/nlohmann-json/` |
| miniz | 3.1.2 source family | Vendored ZIP and CRC implementation | MIT; `licenses/source/miniz/` |
| TinyXML-2 | 11.0.0 | Vendored XML parser | Zlib; `licenses/source/tinyxml2/` |
| Catch2 | 2.13.10 | Vendored test framework; not required by the application runtime | BSL-1.0; `licenses/source/Catch2/` |

The nlohmann/json copyright notice is Copyright (c) 2013-2023 Niels
Lohmann and the other contributors identified in its amalgamated source.
The miniz notice credits RAD Game Tools, Valve Software, Rich Geldreich,
Tenacious Software LLC, Martin Raiber, and the contributors identified in its
source. TinyXML-2's original code is by Lee Thomason. Catch2's amalgamated
header carries the notices of Two Blue Cubes Ltd. and the Catch2 authors.

## Emulation runtime distributed beside Goliath

These are separate upstream works. Goliath communicates with them through
their public executable and API interfaces.

| Component | Audited version | License | License material |
| --- | --- | --- | --- |
| Jolly Good API (JG) | 2.0.0 | Zlib | `licenses/upstream/jg/` |
| Jolly Good Reference Frontend (JGRF) | 2.0.1 | BSD-3-Clause | `licenses/upstream/jgrf/LICENSE` |
| Geolith | 0.4.2 | BSD-3-Clause, with additional bundled notices | `licenses/upstream/geolith/` |
| libchdr | 0.3.0 | BSD-3-Clause | `licenses/upstream/libchdr/LICENSE.txt` |
| libchdr LZMA SDK | 25.01 | Public domain | `licenses/upstream/libchdr/lzma-LICENSE` |
| libchdr miniz | 3.1.1 | MIT | `licenses/upstream/libchdr/miniz-LICENSE` |
| libchdr zstd | 1.5.7 | BSD-3-Clause selected from its BSD/GPL dual license | `licenses/upstream/libchdr/zstd-LICENSE` |

Geolith's `LICENSES` file contains the notices for code incorporated into the
core, including its Z80 implementation and other credited components. JGRF's
copy of miniz is covered by `licenses/upstream/jgrf/miniz-LICENSE`.

The audited Windows build contains documented compatibility changes to JGRF
2.0.1 and libepoxy 1.5.10. Their provenance, patch boundary, validation, and
recovery procedure are recorded in
`docs/WINDOWS_OPENGL_ES_WGL_RUNTIME_FIX.md`. Those projects retain their
respective upstream licenses.

## JGRF shaders and icon

The following JGRF shader families are MIT-licensed; the consolidated license
and copyright notices are in
`licenses/upstream/jgrf/SHADERS-MIT.txt`:

- `aann`: Copyright (c) 2015 nyanpasu64 and wareya;
- `crt-yee64`: Copyright (c) 2017 Lucas Melo;
- `crtea`, `default`, `lcd`, and `sharp-bilinear`: Copyright (c) 2020-2022
  Rupert Carmichael.

Compiled `.spv` files retain the licenses of their corresponding shader
sources. JGRF shader files without a separate notice remain covered by the
JGRF project license.

The Jolly Good icon metadata identifies “A Jolly Good Cuppa,” created by
Rupert Carmichael and published through Openclipart in 2013. It is treated as
CC0-1.0/public-domain material. The retained credit and CC0 text are in
`licenses/upstream/jgrf/`.

Goliath's own application icon was generated with ChatGPT/ImageGen at the
project author's direction. It is not derived from the Jolly Good icon.

## Windows runtime packages

The Windows frontend audited from GitHub Actions run `35733913007` uses the
following MSYS2 UCRT64 runtime packages. CI checks these package versions
before staging subsequent Windows archives.
The staged Windows archive contains the installed license directories of its
runtime DLL packages under `licenses/msys2/`; alternative license texts are
preserved where an upstream package supplies more than one option.

| Component | Audited package version | Selected/disclosed license |
| --- | --- | --- |
| Qt Base | 6.11.2-2 | LGPL-3.0-only for the redistributed Qt DLLs and plugins; Qt's bundled third-party material retains its own license terms |
| SDL3 | 3.4.16-1 | Zlib |
| libb2 | 0.98.1-3 | CC0-1.0 |
| Brotli | 1.2.0-1 | MIT |
| bzip2 | 1.0.8-4 | bzip2 license |
| double-conversion | 3.4.0-1 | BSD-3-Clause |
| libffi | 3.8.0-1 | MIT |
| FreeType | 2.14.3-1 | FreeType License selected from its FTL/GPL dual license |
| GCC runtime libraries (libgcc, libstdc++) | 16.2.0-4 | GPL-3.0-or-later WITH GCC-exception-3.1 and applicable LGPL terms |
| GLib | 2.90.0-1 | LGPL-2.1-or-later |
| Graphite2 | 1.3.15-1 | LGPL-2.1-or-later |
| HarfBuzz | 14.5.0-1 | MIT |
| GNU libiconv/libcharset | 1.19-1 | LGPL-2.1-or-later; package documentation retains its separate terms |
| libjpeg-turbo | 3.2.0-1 | BSD-style terms with the complete IJG and other upstream notices retained |
| ICU | 78.3-4 | ICU license |
| gettext runtime | 1.0-1 | LGPL-2.1-or-later for the runtime library; all package notices are retained |
| md4c | 0.5.3-1 | MIT |
| PCRE2 | 10.48-3 | BSD-3-Clause with the package's binary-like-packages exception where applicable |
| libpng | 1.6.58-1 | libpng license |
| libwinpthread | 14.0.0.r420.g61d40c4c0-1 | MIT and BSD-3-Clause-Clear |
| zstd | 1.5.7-2 | BSD-3-Clause selected from its BSD/GPL dual license |
| zlib | 1.3.2-2 | Zlib |

The audited Qt deployment contains `Qt6Core.dll`, `Qt6Gui.dll`,
`Qt6Network.dll`, `Qt6Widgets.dll`, and the plugins selected by
`windeployqt6.exe` under `generic/`, `imageformats/`, `networkinformation/`,
`platforms/`, `styles/`, and `tls/`. They are dynamically linked and may be
replaced by the user with interface-compatible builds. The Qt Base 6.11.2-2
package's full installed license set is in `licenses/msys2/qt6-base/`. This
package does not install an SPDX document. The corresponding Qt Base upstream
source, MSYS2 recipe, and patches are available in
`mingw-w64-qt6-base-6.11.2-2.src.tar.zst` alongside the release.

Goliath's Windows build statically links eligible GCC runtime and winpthreads
code under their runtime-library exceptions and license terms. JGRF and
Geolith have their own runtime dependency relationships; the notices above
cover the files redistributed with the audited portable package.

The Windows Universal C Runtime, Windows system DLLs, the Vulkan loader
provided by the operating system or graphics driver, and other system
components are not redistributed as Goliath project material.

## Content not distributed

Goliath does not include game ROMs, optical-disc images, BIOS or firmware
files, encryption keys, proprietary game artwork, or external metadata
catalogs. It does not provide download links for that content. Users are
responsible for supplying and using content for which they have the necessary
rights.

Product and project names are used only to identify compatibility or upstream
components. Goliath is an independent project and is not affiliated with,
sponsored by, or endorsed by SNK Corporation, The Qt Company, the Jolly Good
Emulation project, or the other third-party projects identified above.
