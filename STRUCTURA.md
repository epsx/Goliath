# Goliath Qt Project Structure

This document describes the current source layout, build targets, component
boundaries, and principal data flows of `goliath-qt-cpp`. It is an architecture
map for the finished codebase, not a migration guide.

See `README.md` for the project overview, `HOWTO.md` for operational
instructions, and `HISTORY.md` for the chronological development record.

---

## 1. Repository boundary

The intended source repository contains:

```text
goliath-qt-cpp/
├── .github/
│   └── workflows/
│       └── build.yml
├── .gitattributes
├── .gitignore
├── CMakeLists.txt
├── LICENSE
├── THIRD-PARTY-NOTICES.md
├── README.md
├── HOWTO.md
├── HISTORY.md
├── STRUCTURA.md
├── licenses/
├── assets/
├── Screenshots/
├── docs/
├── src/
├── tests/
└── third_party/
```

Root-level patches, backup files, build directories, source/reference archives,
runtime configuration, ROMs, metadata, and generated databases are local
working inputs or outputs. They are excluded by `.gitignore` and are not part of
the canonical source inventory.

`.gitattributes` keeps first-party text files on LF line endings and marks ICO
and ZIP files as binary. `.gitignore` keeps build, runtime, generated-data,
archive, patch, editor-backup, and operating-system artifacts out of commits.

### Root files

| File | Responsibility |
| --- | --- |
| `CMakeLists.txt` | Defines the shared library, application, and test targets. |
| `LICENSE` | GNU GPLv3 license text for Goliath's GPL-3.0-or-later grant. |
| `THIRD-PARTY-NOTICES.md` | Audited third-party component and license inventory. |
| `README.md` | Project purpose, capabilities, architecture summary, and limits. |
| `HOWTO.md` | Build, setup, deployment, rescan, validation, and troubleshooting. |
| `HISTORY.md` | Chronological implementation and cleanup record. |
| `STRUCTURA.md` | Detailed component ownership and data-flow map. |
| `.github/workflows/build.yml` | Windows/Linux CI, tests, dependency and license-coverage audits, and frontend build artifacts. |
| `licenses/` | Exact source, upstream, MSYS2 runtime, and Qt SBOM license materials. |
| `Screenshots/` | Public, content-free application screenshots used by `README.md`. |
| `docs/` | Release-compliance and compatibility-patch records. |

---

## 2. CMake target graph

The application and regression suite share one first-party static library:

```text
Qt6::Core + SDL3 + reusable source + parser source
                         |
                         v
                  goliath-core
                   /          \
                  v            v
         goliath-qt        goliath-qt-tests
         Qt6::Widgets      Catch2 + Qt6::Core
```

### `goliath-core`

`goliath-core` contains logic that can be reused by the application and tested
without constructing the main window:

- configuration primitives;
- guarded regular-log truncation;
- SHA-1 and persistent hash cache;
- filesystem discovery and long-path I/O helpers;
- metadata parsers and Neo Geo CD verification;
- game-model validation;
- exact-media profile/playtime/Favorites identity and safe save-data backup
  logic;
- input maps and SDL3 joystick enumeration/capture;
- audio-device primitives;
- JGRF executable, launch-policy, and Geolith capability helpers;
- BIOS verification;
- the integrated database scanner;
- miniz and tinyxml2 implementation files.

It links `Qt6::Core` and `SDL3::SDL3`. Windows builds also link `ole32` and
`uuid` for Core Audio support.

### `goliath-qt`

The application target adds:

- `src/main.cpp`;
- Qt Widgets UI construction and behavior;
- application path state, theme generation, and file logging;
- session diagnostic controls and live frontend-log clearing;
- launch-time audio handling;
- detached JGRF process management;
- save-data inventory/backup/restore presentation and process guard;
- JGRF BIOS-path preparation;
- rescan thread/dialog integration;
- Qt and Windows icon resources.

It links `goliath-core` and `Qt6::Widgets`. On MinGW, libgcc, libstdc++, and
winpthread are linked statically for the application while Qt and SDL3 remain
dynamic.

### `goliath-qt-tests`

The test target links `goliath-core`, `Qt6::Core`, and the vendored Catch2
header. It is kept as a console executable on MinGW so test output remains
visible.

Tests are controlled by the `BUILD_TESTS` CMake option and are disabled by
default. Developer and CI builds enable them explicitly with
`-DBUILD_TESTS=ON`. CMake also enables Qt AUTOMOC and AUTORCC for the relevant
targets.

---

## 3. Source tree overview

```text
src/
├── main.cpp
├── audio/
│   ├── audio_devices.hpp/.cpp
│   └── jollygood_audio.hpp/.cpp
├── common/
│   ├── debug_logger.hpp/.cpp
│   ├── goliath_common.hpp/.cpp
│   ├── log_file.hpp/.cpp
│   ├── paths.hpp/.cpp
│   ├── sha1.hpp/.cpp
│   └── theme.hpp/.cpp
├── game/
│   ├── audio_export.hpp/.cpp
│   ├── bios_verify.hpp/.cpp
│   ├── db_scanner.hpp/.cpp
│   ├── detached_process_tracker.hpp/.cpp
│   ├── filesystem_io.hpp/.cpp
│   ├── game_model.hpp/.cpp
│   ├── game_library_state.hpp/.cpp
│   ├── game_playtime.hpp/.cpp
│   ├── game_profile.hpp/.cpp
│   ├── game_profile_runtime.hpp/.cpp
│   ├── video_settings.hpp/.cpp
│   ├── geolith_capabilities.hpp/.cpp
│   ├── jollygood_benchmark.hpp/.cpp
│   ├── jollygood_bios.hpp/.cpp
│   ├── jollygood_capabilities.hpp
│   ├── jollygood_executable.hpp/.cpp
│   ├── jollygood_launch.hpp/.cpp
│   ├── jollygood_process.hpp/.cpp
│   ├── neocd_verification.hpp/.cpp
│   ├── neogeo_metadata.hpp/.cpp
│   ├── rescan_worker.hpp/.cpp
│   ├── save_data_manager.hpp/.cpp
│   └── sha1_cache.hpp/.cpp
├── ini/
│   └── ini_document.hpp/.cpp
├── input/
│   ├── input_maps.hpp/.cpp
│   ├── joystick_listener.hpp/.cpp
│   └── sdl_joystick.hpp/.cpp
└── ui/
    ├── main_window.hpp
    ├── main_window.cpp
    ├── main_window_ui.cpp
    ├── main_window_library.cpp
    ├── about_dialog.hpp/.cpp
    ├── audio_export_dialog.hpp/.cpp
    ├── benchmark_dialog.hpp/.cpp
    ├── game_profile_dialog.hpp/.cpp
    ├── logging_dialog.hpp/.cpp
    ├── rescan_dialog.hpp/.cpp
    ├── save_data_dialog.hpp/.cpp
    ├── settings_dialog.hpp/.cpp
    ├── tabs/
    │   ├── audio_tab.hpp/.cpp
    │   ├── core_tab.hpp/.cpp
    │   ├── info_tab.hpp/.cpp
    │   ├── misc_tab.hpp/.cpp
    │   └── video_tab.hpp/.cpp
    └── widgets/
        ├── controller_button.hpp/.cpp
        ├── game_input_mapping_widget.hpp/.cpp
        ├── input_panel_widgets.hpp/.cpp
        ├── optional_video_fields.hpp/.cpp
        ├── settings_field.hpp/.cpp
        └── title_bar.hpp/.cpp
```

The directory name describes the functional domain. CMake target ownership is
based on whether the implementation is reusable/testable core logic or
application-specific UI/process orchestration.

---

## 4. Entry point and common infrastructure

### `src/main.cpp`

The entry point:

1. creates `QApplication`;
2. applies the Fusion style and embedded application icon;
3. initializes `goliath-qt-debug.log` beside the executable;
4. loads or creates `goliath.ini`;
5. constructs and shows `MainWindow`.

### `src/common/`

| Module | Target | Responsibility |
| --- | --- | --- |
| `goliath_common` | core | `Config`, defaults, `goliath.ini`, executable base, and paths. |
| `log_file` | core | Refuses unsafe targets and truncates a direct regular runtime log. |
| `sha1` | core | Lowercase SHA-1 helpers for memory and files. |
| `paths` | app | Resolves `AppPaths`, JGRF arguments, database, config, BIOS, and data paths. |
| `debug_logger` | app | Thread-safe startup/runtime log with info, warning, error, and live clear operations. |
| `theme` | app | Six built-in themes and complete Qt stylesheet generation. |

`get_base_dir()` resolves the directory containing the running executable. All
relative application paths are based there, never on the shell's current
working directory.

`Config` is intentionally small and is used only for Goliath's own
`goliath.ini`. JGRF and Geolith configuration use `IniDocument`, described in
section 8.

---

## 5. Audio components

```text
audio/
├── audio_devices.hpp/.cpp      reusable SDL3/Core Audio primitives
└── jollygood_audio.hpp/.cpp    launch-time UI/process orchestration
```

### `audio_devices`

This core module:

- lazily initializes the SDL3 audio subsystem;
- enumerates available playback devices;
- reports device status to Settings and launch preflight code;
- clamps the Goliath-side volume percentage;
- applies volume to a matching Windows Core Audio process session.

Stock JGRF continues to open SDL's system-default playback device. The physical
device list is informational and does not introduce a private JGRF output
selector.

### `jollygood_audio`

This application module:

- asks the user whether to continue when no playback device is detected;
- retries Windows session-volume application after the detached JGRF process
  starts;
- logs success or final retry failure.

It belongs to the application target because it owns Qt dialogs, timers, and
launch-process context.

---

## 6. Game, scanner, launch, and BIOS components

### Module ownership

| Module | Target | Responsibility |
| --- | --- | --- |
| `audio_export` | core | Sanitizes portable WAV suggestions, adds collision-free suffixes, and validates non-overwriting output targets. |
| `game_model` | core | Defines `Game`/`Rom` and validates `games.json`. |
| `game_library_state` | core | Validates and atomically persists exact-media Favorites and ratings independently from `games.json`. |
| `game_playtime` | core | Validates, formats, and atomically persists exact-media playtime statistics. |
| `game_profile` | core | Persists and validates exact-media launch/video/input overrides independently from `games.json`. |
| `game_profile_runtime` | core | Composes short isolated JGRF configuration trees and layered input mappings for profiled launches. |
| `save_data_manager` | core | Resolves stock JGRF save/state identities and safely inventories, snapshots, validates, restores, and deletes their bounded file set. |
| `video_settings` | core | Owns the shared JGRF/Geolith video keys, labels, defaults, choices, ranges, and validation used by global and per-game UI. |
| `db_scanner` | core | Orchestrates media discovery, metadata, verification, and JSON output. |
| `filesystem_io` | core | Recursive CD discovery and Windows extended-length I/O paths. |
| `neogeo_metadata` | core | MAME/Redump/INI/history parsing and title matching. |
| `neocd_verification` | core | Complete Redump set and MAME set content verification. |
| `sha1_cache` | core | Persistent Redump BIN-track hash cache and fingerprint validation. |
| `bios_verify` | core | BIOS ZIP presence, member, and known CRC32 checks. |
| `jollygood_executable` | core | Canonical configured JGRF executable resolution. |
| `jollygood_capabilities` | header-only | Parses JGRF version, Vulkan help, and JG API version. |
| `geolith_capabilities` | core | Loads the installed Geolith core and queries the JG ABI. |
| `jollygood_benchmark` | core | Builds transient benchmark arguments, parses completion, resolves displayed overrides, and calculates metrics. |
| `jollygood_launch` | core | Selects media and prepares validated launch state. |
| `jollygood_process` | app | Starts detached JGRF and redirects both output streams. |
| `detached_process_tracker` | app | Observes exact detached process objects without controlling them and reads Windows termination status when permitted. |
| `about_dialog` | app | Presents Goliath identity/build metadata, authorship, upstream acknowledgements, and verified external links. |
| `audio_export_dialog` | app | Selects, probes, and confirms a new one-shot WAV destination without creating the requested target. |
| `benchmark_dialog` | app | Supervises, times, cancels, logs, and reports a selected-media JGRF benchmark. |
| `logging_dialog` | app | Owns session verbose launch state and guarded open/clear controls for both runtime logs. |
| `save_data_dialog` | app | Presents exact-media save/state inventory, validated backups, guarded mutation, and folder access. |
| `jollygood_bios` | app | Prepares the BIOS path layout expected by stock JGRF. |
| `rescan_worker` | app | Runs the core scanner on a `QThread` and emits UI signals. |

### `game_model`

`Game` and `Rom` mirror the scanner/frontend JSON contract. Required game
fields are `name`, `display`, `short`, `source`, and `roms`. The model also
validates:

- `system` values (`neogeo` or `neogeocd`);
- source/identity combinations;
- optional CD verification values;
- required ROM fields;
- `main_rom` references to a row whose `main` flag is true.

Malformed game rows are rejected before reaching the UI. Invalid ROM rows are
skipped, and invalid `main_rom` references are cleared.

### Scanner decomposition

`db_scanner` remains the coordinator. Specialized work is delegated to:

- `filesystem_io` for logical CD discovery and platform-safe I/O paths;
- `neogeo_metadata` for `neogeo.xml`, `neocd.xml`, Redump DAT, supplemental
  INI, and history parsing;
- `neocd_verification` for physical CUE/CHD identity checks;
- `sha1_cache` for reusable CUE track hashes;
- `sha1` for actual file hashing.

The scanner itself owns:

- configured-path resolution;
- non-recursive MVS/AES `.neo` enumeration;
- parent, clone, hack, homebrew, and canonical-parent assembly;
- recursive CUE/CHD scan coordination;
- metadata assignment and fallback policy;
- scan statistics, cancellation checks, and progress output;
- final `database/games.json` serialization.

There is no separate `goliath-db` executable.

### `filesystem_io`

The module preserves two path forms:

- logical paths for configuration, JSON, cache keys, UI, and relative results;
- extended-length paths only for Windows filesystem operations that need them.

Neo Geo CD discovery returns relative `.cue` and `.chd` paths and ignores track
or archive files as independent games.

### `neogeo_metadata`

This module owns data structures and parsers for:

- MAME cartridge and CD software lists;
- MAME CD disk SHA-1 indexing;
- Redump Neo Geo CD DAT discovery and parsing;
- `catver.ini`, `catlist.ini`, `genre.ini`, `nplayers.ini`, and `series.ini`;
- `history.xml`;
- normalized MAME/Redump title matching;
- the small cartridge compatibility layer retained from the established
  Goliath database behavior.

It parses trusted metadata but does not grant content-verification status.

### `neocd_verification` and `sha1_cache`

CUE verification safely resolves referenced tracks, rejects parent traversal,
supports case-only recovery where appropriate, and requires matching size plus
SHA-1 for the Redump entry.

CHD verification reads the v4/v5 internal combined SHA-1 and compares it with
the MAME `neocd.xml` disk catalog. It does not use the ordinary whole-file hash.

`Sha1Cache` reuses a track hash only when normalized logical path, byte size,
and modification time all match. The scanner controls conservative pruning so
an unavailable CD root or Redump catalog does not erase valid cache state.

### Launch decomposition

Launch responsibilities are deliberately separated:

- `jollygood_executable` gives every caller identical Windows `.exe` handling;
- `jollygood_capabilities` parses information printed by the configured JGRF;
- `geolith_capabilities` loads exactly
  `cores/geolith/geolith.<dll|so|dylib>` beside that JGRF and queries JG symbols;
- `jollygood_launch` resolves selected media, checks CHD capability, builds the
  environment/arguments, appends optional session verbose and one-shot WAV
  options, rejects configured writer conflicts, and returns a status without
  showing UI;
- `jollygood_benchmark` inserts JGRF's transient `-b <frames>` before the final
  media argument, parses its completion marker, resolves displayed CLI
  overrides, and calculates deterministic throughput metrics;
- `MainWindow` converts status into user-facing decisions;
- `jollygood_process` launches JGRF detached and appends stdout/stderr to
  `jollygood.log` for normal gameplay;
- `detached_process_tracker` retains an exact Windows process handle or Linux
  `pidfd` where available, while one `MainWindow` timer polls all active normal
  launches for playtime accounting;
- `benchmark_dialog` owns the exceptional managed JGRF child used for timing,
  cancellation, completion validation, log capture, and result presentation.

The optional session verbose flag is appended only to normal launch
preparation and remains before the final media argument. It is deduplicated
against `-v`/`--verbose` in the configured base arguments. Benchmark launch
preparation deliberately does not request the session flag; explicitly
configured base arguments are still preserved.

Only the final external Neo Geo CD media argument is converted to a Windows
extended-length path when necessary. Stored and displayed paths stay logical.
An explicit WAV destination receives the same conversion only when that
absolute output path itself reaches `MAX_PATH`.

### Per-game profiles

`GameProfileStore` keys every record by system plus normalized relative media
path. This keeps cartridge parents and variants separate and also distinguishes
CUE from CHD copies of the same title. Records live in
`config/game_profiles.json`, outside scanner-generated `games.json`.

For a profiled launch, `game_profile_runtime` copies the current upstream INIs
into `config/p/<64-bit-hash>/jollygood/`, overlays only the requested fields,
and returns that short directory as the child process's `XDG_CONFIG_HOME`.
`XDG_DATA_HOME` remains global, so BIOS, saves, states, and cheats are shared.
The module rejects a path that would overflow JGRF 2.0.1's fixed 128-byte
configuration-path buffer.

For an active profile, the library context menu can invoke the same runtime
composer before opening that exact media's generated `jollygood/` directory.
This avoids exposing hash construction in the UI and prevents stale generated
INI copies from being presented as the current effective configuration.

Core constraints are enforced both in the dialog and at the runtime boundary:
Free Play/Setting Mode require MVS hardware, four-player requires direct MVS
plus JP/AS, and unsupported CD input modes normalize to Auto. A controller
override retargets inherited Player 1/single-player `jN...` mappings while
leaving keyboard, Player 2+, and unrelated sections unchanged. Exact-media
input mappings are validated and layered afterward, so an explicit captured
`jN...` binding retains its own port. If the global input INI is absent, the
composer seeds the isolated copy from built-in defaults before adding custom
mappings.

Frontend advanced video values are stored as validated `jgrf_video` entries
and overlaid in the isolated `[video]` section. Geolith aspect, palette, and
overscan values use validated `geolith_video` entries and isolated `[geolith]`.
Video API and shader retain their compatible top-level profile fields and final
CLI precedence; fullscreen/windowed and scale are also appended last so stale
global launch arguments cannot defeat an exact-media selection.

### Save-data manager

`save_data_manager` reproduces the upstream JGRF data contract instead of
inventing its own live filenames. It derives `gamename[128]` from the final
media component, truncates it to 127 bytes, and then strips the last extension.
It resolves dynamic-core `data/jollygood/{state,save}/geolith` paths and the
platform-specific static-core layout. Only `.st0`, `.st1`, `.nv`, `.srm`,
`.mcr`, `.brm`, and `.dip` are managed.

Goliath-owned manual snapshots are separated by the same stable exact-media
hash used for bounded profile storage. A ZIP manifest records the full
normalized system/media identity and every member. Restore validates the
archive's direct location, regular-file status, member names/counts, supported
compression, lack of encryption, bounded sizes, manifest agreement, and CRC
before mutation. Current payloads are retained in memory and written to a
protective archive first; a partial apply triggers rollback. Live deletion is
also preceded by a protective snapshot. No arbitrary rename/path interface or
automatic launch backup exists.

The exact-media hash isolates backup archives, not upstream live data. Stock
JGRF still keys live state/save files by its bounded basename, so two different
media paths with the same result share those files by design.

### BIOS components

`bios_verify` is deterministic core logic. It reads the known BIOS ZIP sets
with miniz and validates required members and known CRC32 values.

`jollygood_bios` performs runtime integration. It prepares the BIOS location
expected by unmodified JGRF, including a Windows NTFS junction when appropriate,
safe retargeting, legacy data-directory migration, and a copy fallback.

---

## 7. INI and input components

### `src/ini/ini_document`

`IniDocument` is an order-preserving, case-preserving reader/writer for:

```text
config/jollygood/settings.ini
config/jollygood/geolith.ini
config/jollygood/geolith_input.ini
```

It updates managed settings without discarding unrelated upstream sections,
keys, comments, ordering, or key case. This is separate from Goliath's simpler
`Config` representation for `goliath.ini`.

### `src/input/`

| Module | Responsibility |
| --- | --- |
| `input_maps` | Canonical keys/defaults, SDL names, normalization, bounded JGRF binding validation, and hotkeys. |
| `sdl_joystick` | Shared SDL3 initialization and connection-order device enumeration. |
| `joystick_listener` | One-shot button, axis, or hat capture on a worker thread. |

`input_maps` also distinguishes directionless analog-axis bindings from axes
used as digital buttons, maps frontend-only device profiles to valid Geolith
values, detects keyboard bindings that overlap JGRF hotkeys, and rejects input
codes that would exceed JGRF 2.0.1's fixed keyboard/axis/button/hat arrays.
`joystick_listener` limits capture to those same six axes and 32 buttons.

SDL3 is used so Goliath's joystick ordering follows the same connection-order
model as JGRF. Device ports can still change if hardware is unplugged and
reconnected in a different order before launch.

---

## 8. UI components

### Main window split

One `MainWindow` class is implemented across three translation units:

| File | Responsibility |
| --- | --- |
| `main_window.cpp` | Lifecycle, paths, launch preflight, Settings, rescan, and responsive layout. |
| `main_window_ui.cpp` | UI construction, custom title bar, branch assets, and themes. |
| `main_window_library.cpp` | Library views, exact-media Rating/Playtime sort and filters, search, details, variants, Random, and actions. |

The main content uses a `QSplitter`. The details side collapses below the narrow
window threshold and restores its previous proportion when sufficient width
returns.

The top toolbar keeps library controls and Random on the left, then a stretch
followed by Tools, Settings, and About on the right. The title-bar close button
retains the existing `closeEvent` lifecycle; there is no duplicate Exit button.

Library state deliberately preserves selection across applicable view rebuilds,
while an explicit Sort change selects and reveals its first ranked exact-media
result. Search and expanded parent groups remain preserved. The exact selected
parent, variant, CUE, or CHD can be marked through the details button or context
menu; its marker and the persistent **Favorites only** filter use
`game_library_state.json`, not scanner output. Toggling that marker also
restores the current tree viewport unless the Favorites-only view must remove
the selected item.
Expand-all and collapse-all preserve the viewport's top visible library region
independently from selection. When collapse-all hides a selected variant, its
parent becomes selected without replacing the viewport anchor.
Before replacing the scanned game model, a completed rescan captures the
current exact-media selection and restores it against the rebuilt model;
`last_rom` remains the startup fallback rather than the rescan target.
The same exact-media record owns an independent 1–5-star rating; clearing a
Favorite preserves its rating and clearing a rating preserves its Favorite.
Rating and Playtime sorts keep missing values last in both directions, rank a
parent group by its best exact-media value, sort variants by their own values,
and use the display name as a stable tie-break. The compact Filters menu
combines Rating and Playtime predicates and persists both choices in
`goliath.ini`. Exact matching variants remain visible through their parent
container even when normal variants are hidden; container-only parents are not
eligible selections. Parent expansion performed only to expose an exact filter
match is tagged as temporary and excluded from the user expansion state carried
across rebuilds. Random selects visible parents normally and exact media matching
Search, Favorites, and all active personal filters in a restrictive view. The
details panel presents Playtime and Sessions separately with complete tooltips.
The search line edit uses Qt's native trailing clear action;
Escape is widget-scoped so it clears only a focused search field, while Ctrl+F
focuses and selects the current query.
The Theme, Sort, and Settings combo views and their separate popup containers
receive the same generated palette explicitly, preventing native Windows frame
colors from appearing above or below their item views. Settings applies the
shared popup helper once to every combo descendant across all tabs.

`game_profile_dialog` edits the profile for the exact selected media. Its
**General** tab owns system/input launch settings, its generated **Video** tab
uses the same authoritative schema as global Settings, and
`game_input_mapping_widget` owns an **Input Mapping** tab that reuses the
visual input panels and captures keyboard or bounded SDL inputs. CRTea custom
controls follow upstream preset semantics, and video-only reset does not alter
other profile fields. Every unchanged control inherits the global mapping;
custom controls are removable individually or by displayed mapping profile.
**Reset to Global** removes the whole record. The dialog dynamically disables
MVS-only settings, validates four-player mode, and explains the connection-
order limitation of JGRF controller ports. Active profiles receive a gear
marker in the library.

The library context menu selects the clicked row before opening, and its
Benchmark action calls the same `benchmarkSelected` slot as the Tools menu.
`benchmark_dialog` runs the selected media through the same launch preparation
and optional exact-media runtime profile, but keeps the child `QProcess`
attached for the duration of the measurement. The benchmark flag is transient;
the dialog never writes global INIs or `game_profiles.json`.

Normal detached launches are separately observed for playtime. Closing the
main window while JGRF is active hides the UI but keeps Goliath as a background
observer on Windows and Linux; it saves the complete observed session and exits
after the final tracked process ends. The main window updates
`GamePlaytimeStore` only after launch validation, stores the exact-media total
atomically, and refreshes Playtime/Last Played details. Windows loader failures
such as missing or incompatible DLLs are rejected from playtime by exit status
even when their operating-system dialog outlives the normal duration guard.
Benchmark processes are managed by `benchmark_dialog` and never enter this
tracking path. Observation remains non-owning and never signals or terminates
the detached game.

`save_data_dialog` is opened for the exact selected row from Tools or the
context menu. It displays both exact media and the upstream JGRF basename,
keeps invalid/foreign archives visible but non-restorable, and provides state,
save, and backup folder actions. Its mutation callback treats every live or
unknown process tracked by the current `MainWindow` as active, making backup,
restore, and delete read-only until termination is positively observed.

`logging_dialog` owns no persistent configuration. It returns one session
toggle to `MainWindow`, reports and opens both runtime logs, clears the active
frontend log through `DebugLogger`, and delegates standalone JGRF-log
truncation to `log_file`. JGRF-log clearing uses the same conservative tracked-
process callback as save-data mutations; external or restart-surviving
processes remain an explicit user responsibility.

### Settings dialog

`settings_dialog` creates and coordinates seven tabs:

```text
Video | Audio | Misc | Core | Input | Path | Info
```

Dedicated tab classes own Video, Audio, Misc, Core, and Info. The dialog itself
owns Path selection and the visual Input workflow because those operations
coordinate multiple widgets, configuration files, and capture threads. Input
capture always accepts the keyboard and optionally polls the selected SDL
controller in parallel. A single cancellation path restores the waiting
control and stops polling when the user changes context, saves, resets, or
closes the dialog.

### Settings tabs

| Module | Responsibility |
| --- | --- |
| `video_tab` | JGRF/Geolith video fields and asynchronous Vulkan capability probing. |
| `audio_tab` | JGRF audio fields, asynchronous device status, and Goliath volume. |
| `misc_tab` | JGRF `[misc]` settings. |
| `core_tab` | Geolith system, input, memory-card, and miscellaneous core settings. |
| `info_tab` | Lazy/asynchronous JGRF, Geolith, JG API, format, CHD, and Vulkan information. |

The asynchronous probes prevent Settings construction from blocking on slow
audio endpoints, process help output, or shared-library loading.

### About dialog

`about_dialog` is opened from the **About** button at the right end of the
toolbar, following **Tools** and **Settings**. It reuses the shared frameless
title bar and current
main-window stylesheet, shows the embedded application icon and configure-time
build identity, and owns only product credits and external links. Runtime
JGRF/Geolith/JG capability probing remains isolated in Settings -> Info.
Repository and license buttons require explicit build-time URLs and stay
disabled when those destinations have not been published.

### Rescan dialog

`rescan_dialog` is the modal progress surface for `RescanWorker`. It shows live
scanner messages, an indeterminate progress bar, the final summary, errors, and
cancellation/close behavior without running scanner logic itself.

### Reusable widgets

| Module | Responsibility |
| --- | --- |
| `title_bar` | Frameless title bar, window drag, system resize, and dialog setup. |
| `settings_field` | Shared field specifications, validation, and Qt controls for settings tabs. |
| `controller_button` | Colored two-line input-mapping button. |
| `input_panel_widgets` | Visual Neo Geo, Mahjong, V-Liner, Irritating Maze, and System panels. |

`InputPanelWidget` gives Settings one common interface for device-specific
panels. Free helper functions bridge specialized `ControllerButton` instances
and fallback `QPushButton` rows. The shared System panel keeps its single
**Cabinet** group at its content width, so widening Settings or exact-media
Input Mapping adds free space without stretching the control background.

---

## 9. Assets

```text
assets/
├── app_icon.qrc
├── app_icon.rc.in
├── goliath_icon.png
└── goliath-qt.ico
```

- `app_icon.qrc` embeds the icon in Qt resources for application windows;
- `app_icon.rc.in` is configured from the CMake project version and assigns
  the Windows executable icon and version metadata;
- `goliath_icon.png` is the canonical high-resolution source artwork with a
  transparent background;
- `goliath-qt.ico` is the derived multi-image asset used by Qt and Windows.

The retired one-off icon generator is not part of the repository. The checked
PNG source and derived ICO are both checked in, so no image-generation tool is
required during a normal build.

---

## 10. Tests

```text
tests/
├── test_main.cpp
├── test_audio.cpp
├── test_audio_export.cpp
├── test_bios_verify.cpp
├── test_db_scanner.cpp
├── test_game_model.cpp
├── test_game_library_state.cpp
├── test_game_playtime.cpp
├── test_game_profiles.cpp
├── test_game_system.cpp
├── test_ini_document.cpp
├── test_input_maps.cpp
├── test_jollygood_benchmark.cpp
├── test_log_file.cpp
├── test_save_data_manager.cpp
├── test_settings_fields.cpp
├── test_sha1.cpp
└── third_party/
    └── catch2/
        └── catch.hpp
```

### Test-file ownership

| File | Coverage |
| --- | --- |
| `test_main.cpp` | Catch2 test runner entry point. |
| `test_audio.cpp` | Volume bounds, device snapshot state, and default config. |
| `test_audio_export.cpp` | Portable names, `.wav` normalization, collision avoidance, non-destructive target validation, configured-writer conflicts, and final argument placement. |
| `test_bios_verify.cpp` | BIOS catalog, missing paths/archives/members, and optional CD sets. |
| `test_db_scanner.cpp` | Grouping, metadata, CD verification, cache, path safety, and statistics. |
| `test_game_model.cpp` | Required fields, invalid rows, `main_rom`, and optional metadata. |
| `test_game_library_state.cpp` | Exact-media Favorite/rating identity, v1-to-v2 migration, validation, atomic persistence, independent removal, and rescan survival. |
| `test_game_playtime.cpp` | Exact-media accumulation, formatting, launch/loader validation, atomic persistence, failures, and overflow saturation. |
| `test_game_profiles.cpp` | Exact-media keys, authoritative video schema, validated video/input JSON, isolated layered INIs/defaults, constraints, controller retargeting, and path limits. |
| `test_game_system.cpp` | System/source/identity compatibility and legacy defaults. |
| `test_ini_document.cpp` | Load, edit, remove, preservation, and save round trips. |
| `test_input_maps.cpp` | Input normalization, profiles, bounded bindings, analog definitions, and JGRF hotkeys. |
| `test_jollygood_benchmark.cpp` | Benchmark argument placement, completion parsing, displayed option resolution, and throughput metrics. |
| `test_log_file.cpp` | Missing-log idempotence, regular-file truncation, and unsafe-target refusal. |
| `test_save_data_manager.cpp` | JGRF basename/layout parity, bounded inventory, snapshot/restore, protective deletion, exact-media isolation, and malicious/foreign ZIP rejection. |
| `test_settings_fields.cpp` | Field fallback, capabilities, media gating, paths, selection, and profiled launch environment/arguments. |
| `test_sha1.cpp` | Standard SHA-1 vectors and file hashing. |

The authoritative release condition is that the complete configured test suite
passes with no failures. Historical assertion and test-case counts are recorded
in `HISTORY.md`.

The scanner suite uses temporary runtime trees and generated fixture files. Its
negative diagnostics are expected test behavior, not application failures.

---

## 11. Vendored dependencies

### `third_party/`

```text
third_party/
├── json.hpp
├── miniz.h
├── miniz.c
├── miniz_common.h
├── miniz_export.h
├── miniz_tdef.h/.c
├── miniz_tinfl.h/.c
├── miniz_zip.h/.c
└── tinyxml2.h/.cpp
```

- nlohmann/json provides JSON parsing and serialization;
- miniz provides ZIP reading, decompression, and CRC32 for BIOS verification;
- tinyxml2 parses MAME software lists, Redump DAT, and history XML.

Catch2 2.13.10 is vendored separately under `tests/third_party/` because it is
used only by the regression target.

JGRF, JG, Geolith, Qt6, and SDL3 are external build/runtime components, not
vendored project source.

Their audited notices, together with the standalone notices for vendored
source, are preserved under `licenses/` and indexed by
`THIRD-PARTY-NOTICES.md`.

The Windows CI staging step verifies `licenses/SHA256SUMS` and maps every
top-level DLL found in the MSYS2 installation back to its owning package. The
artifact is rejected when `licenses/msys2/` has no matching package license
directory.

---

## 12. Runtime, external, and generated files

The source tree and runtime tree have different ownership:

| Category | Examples | Owner |
| --- | --- | --- |
| Application config | `goliath.ini` | Goliath `Config` |
| Per-game profiles | `config/game_profiles.json` | `GameProfileStore` |
| Per-game playtime | `config/game_playtime.json` | `GamePlaytimeStore` |
| Personal library state | `config/game_library_state.json` | `GameLibraryStateStore` |
| Profile runtime cache | `config/p/*/jollygood/*.ini` | `game_profile_runtime` |
| Exact-media save backups | `data/goliath/save_backups/<hash>/*.zip` | `save_data_manager` |
| One-shot WAV captures | `data/goliath/audio_exports/*.wav` by default | Stock JGRF, selected and preflighted by Goliath |
| Upstream config | `config/jollygood/*.ini` | `IniDocument` and stock JGRF/Geolith |
| External metadata | `metadata/*.xml`, `*.ini`, Redump `.dat` | User-provided scanner input |
| Generated database | `database/games.json` | `db_scanner` |
| Generated hash state | `database/hash_cache.json` | `Sha1Cache` through `db_scanner` |
| Application log | `goliath-qt-debug.log` | `DebugLogger`, with live clear through `logging_dialog` |
| Emulator log | `jollygood.log` | `jollygood_process` or managed benchmark capture; guarded clear through `log_file` |
| Runtime data | `data/jollygood/` | JGRF integration and BIOS preparation |
| User content | `roms/`, `neocd/`, `bios/`, `icons/`, `snaps/` | User/runtime environment |
| Build output | `build/`, `build-*/` | CMake and Ninja |
| Legal payload | `LICENSE`, `THIRD-PARTY-NOTICES.md`, `licenses/` | Copied beside the application by CMake |

None of these runtime/generated paths is required to compile the source. The
external metadata bundle remains user-provided, untracked, and excluded from
the Goliath distribution.

---

## 13. Principal data and control flows

### Startup

```text
main.cpp
  -> initialize DebugLogger
  -> load/create goliath.ini
  -> construct MainWindow
       -> compute AppPaths
       -> load game_profiles.json
       -> load game_playtime.json
       -> load game_library_state.json
       -> load and validate games.json
       -> build UI and library view
       -> apply saved theme/state
```

### Library rescan

```text
Tools / F5
  -> RescanDialog
  -> RescanWorker thread
  -> scan_roms(Config)
       -> load MAME, Redump, INI, and history metadata
       -> enumerate MVS/AES .neo files
       -> recursively discover CD CUE/CHD images
       -> verify content and update hash cache
       -> assemble parent/variant games
       -> write games.json
  -> MainWindow reloads Game/ROM rows
  -> library view and status are rebuilt
```

### CUE verification

```text
CUE logical path
  -> safe FILE reference resolution
  -> filesystem I/O path conversion when required
  -> size + cached/fresh SHA-1 for every track
  -> Redump track identity
  -> CUE size + SHA-1 comparison
       -> exact CUE: redump-cue / ✓ Redump set
       -> different CUE: redump-tracks-only / ⚠ CUE mismatch
```

### CHD verification

```text
CHD logical path
  -> read v4/v5 header
  -> extract internal combined SHA-1
  -> match neocd.xml disk hash
  -> mame-chd / ✓ MAME set
```

### Game launch

```text
selected tree row
  -> selected_launch_media(Game, variant)
  -> exact-media GameProfileStore lookup
  -> prepare_jollygood_launch
       -> resolve configured JGRF
       -> gate CHD through installed Geolith capability
       -> materialize isolated profile INIs when present
       -> overlay advanced video/core values in isolated geolith.ini
       -> append per-game API/fullscreen/scale/shader after global options
       -> create JGRF arguments and environment
  -> audio-device confirmation
  -> BIOS warning and JGRF BIOS-path preparation
  -> detached JGRF process
       -> merge stdout/stderr, then append once to jollygood.log
       -> Windows session-volume retry
       -> exact-process playtime observation
            -> ignore sessions shorter than five seconds
            -> reject validated loader/early-start failures
            -> remain windowless after UI close until every session ends
            -> atomically update config/game_playtime.json
```

### Selected-game benchmark

```text
selected tree row
  -> exact-media profile lookup
  -> normal prepare_jollygood_launch preflight
  -> append transient -b <frames> before media
  -> managed JGRF process
       -> elapsed process timing
       -> stdout/stderr capture + jollygood.log append
       -> completion-marker and exit-status validation
       -> FPS and milliseconds/frame result
       -> optional cancellation
```

### Selected-game WAV export

```text
selected tree row
  -> portable timestamped default below data/goliath/audio_exports
  -> native Save As dialog
  -> enforce .wav + refuse occupied/non-absolute/invalid target
  -> temporary same-directory write probe, removed before launch
  -> normal exact-media launch preparation
       -> reject configured -o/--wave conflict
       -> preserve profile + optional session verbose
       -> append transient --wave <new file> before final media
  -> detached JGRF gameplay
       -> recording survives Goliath close
       -> normal JGRF exit finalizes WAV header
```

### Diagnostics and log clearing

```text
Tools -> Diagnostics & Logs
  -> session verbose checkbox
       -> MainWindow memory only
       -> normal Launch / Random preparation
       -> optional --verbose before final media
  -> frontend log
       -> open direct regular file
       -> DebugLogger synchronized flush/truncate/seek
       -> later frontend writes continue
  -> JGRF log
       -> conservative tracked-process guard
       -> refuse directory/symlink/special target
       -> truncate direct regular file
```

The session checkbox is excluded from the benchmark preparation call. A
verbose option already present in the configured base arguments is not owned
by this toggle and therefore remains effective everywhere those arguments are
used.

### Settings persistence

```text
Goliath Path/UI/volume state
  -> Config
  -> goliath.ini

Video/Audio/Misc/Core/Input settings
  -> IniDocument
  -> config/jollygood/settings.ini
  -> config/jollygood/geolith.ini
  -> config/jollygood/geolith_input.ini

Exact-media Game Settings
  -> GameProfileStore
  -> config/game_profiles.json
  -> inherit global INIs or built-in input defaults
  -> layer launch, advanced video, and exact-media input mappings
  -> generated config/p/<hash>/jollygood/*.ini at launch

Normal game session
  -> detached-process observer
  -> exact system + media key
  -> GamePlaytimeStore
  -> config/game_playtime.json
  -> Playtime / Last Played details

Favorite toggle / rating selection
  -> exact system + media key
  -> GameLibraryStateStore
  -> atomic config/game_library_state.json replacement
  -> Favorite tree marker / filter / Random and details-panel rating stars
```

### Save-data management

```text
selected exact media
  -> reproduce bounded JGRF game name
  -> resolve dynamic/static state and save folders
  -> inventory seven known filenames
  -> manual exact-media ZIP + identity manifest
  -> validate full archive before restore
  -> protective backup
  -> atomic replacement or rollback
```

### Installed-component information

```text
Settings -> Info
  -> JGRF --help process
       -> version and Vulkan availability
  -> Geolith shared-library worker
       -> core version, JG API, systems, formats, CHD capability
  -> read-only UI result
```

---

## 14. Architectural invariants

The current organization follows these rules:

1. Scanner output is never trusted directly by the UI; `game_model` validates
   the JSON boundary first.
2. Reusable logic lives in `goliath-core`; dialogs and process/UI decisions stay
   in the application target.
3. Rescans and Settings device/capability probes run outside the GUI thread so
   their progress cannot freeze normal dialog construction.
4. Goliath uses stock JGRF/JG/Geolith interfaces and does not depend on private
   upstream source modifications.
5. One JGRF executable resolver and one Geolith capability probe are shared by
   Info, launch policy, and tests.
6. Logical paths remain stable in configuration, JSON, cache, and UI; special
   Windows path syntax is limited to external I/O boundaries.
7. CUE and CHD verification use format-appropriate trusted identities and never
   grant badges from filenames alone.
8. Upstream INI files preserve unknown keys, ordering, comments, and key case.
9. External metadata and user content are runtime inputs, not repository source.
10. CMake explicitly lists first-party translation units so application and test
    ownership remain auditable.
11. Contract or logic changes require matching regression coverage.
12. Benchmark mode reuses normal launch policy but remains transient and
    managed; it never changes the detached gameplay process contract or saves
    benchmark state in global/per-game configuration.
13. Playtime observation is exact-media and non-owning: it never terminates
    JGRF, never counts Benchmark or validated loader failures, never writes
    scanner-generated data, and remains active without a visible main window
    until all tracked games finish.
14. Save-data mutation is limited to computed upstream filenames and validated
    direct exact-media archives, always protects live data first, and is
    disabled while a process tracked by the current Goliath session may run.
15. WAV export is explicit, transient, non-overwriting, and exact-media: no
    output path is persisted, Benchmark/Random receive no implicit capture,
    and the requested file is created only by stock JGRF after preflight.
16. Favorites and independent 1–5-star ratings are exact-media personal state
    stored outside `games.json`; rescans may rebuild the library but never
    erase either field.

---

## 15. Documentation map

```text
README.md     project overview, features, architecture summary, and limits
HOWTO.md      build, runtime setup, deployment, validation, and troubleshooting
HISTORY.md    chronological implementation and cleanup history
STRUCTURA.md  source ownership, target boundaries, and data/control flows
```
