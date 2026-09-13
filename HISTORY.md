# Goliath Qt History

This document records the major implementation, validation, refactoring, and
repository-cleanup milestones of `goliath-qt-cpp`. Entries are newest first.

Historical test counts are retained as checkpoint evidence. The current
authoritative baseline is:

```text
All tests passed (2215 assertions in 156 test cases)
```

The preceding `1360/99` Feature 9 baseline passed its per-game mapping,
isolation, and global-configuration regression checks. The `1239/88` runtime
baseline passed manual MVS/AES, Neo Geo CD CUE, Neo Geo CD CHD, Settings, and
visible-only Random smoke tests.

---

## 2026-09-13 — Patch 30A: Windows and Linux continuous integration

- Added GitHub Actions jobs that build and test Goliath on Windows MSYS2
  UCRT64 and Linux.
- Added downloadable frontend build artifacts with license materials and
  dependency reports; the Windows artifact also collects its Qt, SDL3, and
  detected MSYS2 runtime DLLs.
- Kept JGRF, Geolith, BIOS, game media, and user data outside CI artifacts.
- Advanced the development identity to version `0.30.1` and build `patch30a`.

---

## 2026-09-13 — Patch 29C: explicit Linux build instructions

- Added concrete dependency-install commands for Debian/Ubuntu, Fedora, and
  Arch Linux.
- Added standalone Linux configure, build, and launch commands instead of
  referring readers back to the Windows procedure.
- Advanced the development identity to version `0.29.3` and build `patch29c`.

---

## 2026-09-13 — Patch 29B: repository privacy cleanup

- Removed the remaining local workstation name and workspace path from
  historical documentation before the first public repository upload.
- Preserved upstream `COPYING.LIB` license texts from the compiled-library
  ignore rule on case-insensitive filesystems.
- Advanced the development identity to version `0.29.2` and build `patch29b`.

---

## 2026-09-13 — Patch 29A: public README screenshots

- Added three content-free screenshots showing the main dark theme, input
  configuration, and About dialog without personal paths or user-supplied game
  data.
- Added a compact, clickable screenshot gallery to `README.md`.
- Advanced the development identity to version `0.29.1` and build `patch29a`.

---

## 2026-09-13 — Patch 28B: development version alignment

- Aligned the displayed development version with the patch series: Patch 28B
  is Development version `0.28.2`.
- Established the convention `Patch NN[A-Z]` → `0.NN.letter-index`, for
  example Patch 29B → Development version `0.29.2`.

---

## 2026-09-13 — Patch 28A: concise README and opt-in test build

- Replaced the oversized README with a shorter user-facing project overview,
  first-run guide, build instructions, runtime layout, and documentation map.
- Added direct Build Windows and Build Linux links below the project icon.
- Added short Windows staging guidance using `windeployqt6.exe` and `ldd`, and
  clarified that Linux `ldd` audits dependencies but does not create a
  portable package by itself.
- Made `BUILD_TESTS` default to `OFF`; developers and CI enable the regression
  executable explicitly with `-DBUILD_TESTS=ON`.
- Marked `goliath-qt-tests(.exe)` as validation-only and excluded it from the
  normal user payload.
- Linked to the existing JGRF Vulkan and Geolith CHD build instructions in the
  detailed HOWTO instead of duplicating them in the README.

---

## 2026-09-12 — Patch 27C: application icon and published project link

- Replaced the application icon with the approved Goliath artwork without the
  former third-party product text and retained a transparent PNG source.
- Generated a transparent Windows/Qt ICO containing 256, 128, 64, 48, 32, 24,
  and 16 pixel layers.
- Published `https://github.com/epsx/Goliath` as the default About link and
  added a built-in fallback for build directories that cached an empty URL.
- Removed the obsolete repository-not-configured status from About and added
  regression coverage for the canonical project URL.

---

## 2026-09-12 — Patch 27B: resizable About dialog

- Increased the initial About window size so the complete project and license
  summary fits without scrolling at normal desktop DPI.
- Enabled all-side resizing and the corner size grip while retaining the
  scroll area as a fallback for small screens and high display scaling.

---

## 2026-09-12 — Patch 27A: license audit and release compliance

- Selected GPL-3.0-or-later for Goliath's original project material and added
  the complete GPLv3 license text.
- Added an audited third-party inventory covering vendored source, JG, JGRF,
  Geolith, libchdr, shader/icon assets, Qt, and the Windows runtime libraries.
- Preserved the complete installed Qt Base 6.11.1 license set and SPDX SBOM,
  together with the exact upstream and MSYS2 license materials used by the
  audited Windows candidate.
- Added automatic post-build deployment of `LICENSE`,
  `THIRD-PARTY-NOTICES.md`, and `licenses/` beside the application.
- Replaced the pending-license state in About with the GPL grant, copyright,
  no-warranty notice, redistribution/modification notice, and explicit
  Qt/LGPL disclosure.
- Documented corresponding-source and clean-package requirements separately
  from the user-facing README.
- Made the distribution boundary explicit: ROMs, disc images, BIOS/firmware,
  encryption keys, proprietary game artwork, and external metadata catalogs
  are user-supplied and are not Goliath release payloads.
- Added regression coverage for the canonical project license metadata.
- Public release remains gated on an exact-source archive for Goliath and Qt,
  clean Windows build/test results, and final portable-payload verification.

---

## 2026-09-07 — Release Fix 22: portable JGRF launch and low-resolution input settings

- Corrected dynamic-core BIOS preparation on Windows: launches using
  `-c`/`--core` now prepare `XDG_DATA_HOME/jollygood/bios`, while static-core
  launches retain the BIOS directory beside the JGRF executable.
- Passed portable config/data locations below the Goliath working directory to
  JGRF as short relative paths. This avoids JGRF 2.0.1's fixed 64-byte data-path
  buffer without modifying JG, JGRF, or Geolith; externally configured paths
  remain absolute.
- Corrected the default MVS mapping collision by assigning Coin 1 to keyboard
  `5` (SDL scancode 34) and Coin 2 to keyboard `2` (SDL scancode 31).
- Made the entire Settings -> Input page scrollable so every control and the
  Save/Reset actions remain reachable on low-resolution displays.
- Added dynamic/static BIOS-layout, default coin mapping, and portable/external
  launch-environment regression coverage. The complete Windows UCRT64 suite
  passes `2215/156`.
- Public release remains unauthorized while `license_status=pending`.

---

## 2026-09-07 — Cleanup 21: Python residue and dependency audit

- Confirmed that the canonical Goliath source contains no Python source,
  bytecode, package manifests, virtual environments, interpreter invocation,
  CMake Python discovery, or Python runtime dependency.
- Removed the obsolete `/assets/*.py` text-normalization rule left behind
  after the earlier deletion of `assets/gen_ico.py`.
- Preserved accurate historical and dependency-scope documentation, including
  the statement that Meson and Python were build tools for the external
  libepoxy Fix01 reconstruction only and are not Goliath/JGRF runtime
  dependencies.
- Preserved third-party source comments verbatim; references to Python value
  ordering in `third_party/json.hpp` do not introduce Python code or linkage.
- Recorded that the portable distribution must not contain a Python
  interpreter, Python DLL, standard library, bytecode, or package metadata.
- No C++ source, build definition, test, executable, or deployed runtime file
  was changed by this cleanup.

---

## 2026-09-06 — Runtime Fix01: Windows OpenGL ES through WGL

- Corrected the bundled libepoxy 1.5.10 Windows dispatch path so OpenGL ES
  functions provided by the active WGL driver are resolved through
  `wglGetProcAddress`, with the standard `opengl32.dll` fallback for older
  entry points.
- Corrected JGRF 2.0.1 XRGB8888 texture allocation under OpenGL ES by using a
  matching BGRA internal/external format; desktop OpenGL and RGBA paths retain
  their existing formats.
- Preserved JGRF's Vulkan renderer, complete SPIR-V shader set, DLL import set,
  and the unchanged JG 2.0.0, Geolith 0.4.2, SDL3, and Goliath components.
- Recorded isolated dispatch and two-pass rendering probes, reproducible
  baseline/candidate builds, exact-hash deployment with recovery, and
  user-confirmed image and input for MVS/AES plus Neo Geo CD CUE and CHD.
- Documented the external runtime scope, provenance, hashes, evidence, and
  remaining hardware coverage in `docs/WINDOWS_OPENGL_ES_WGL_RUNTIME_FIX.md`.

---

## 2026-09-06 — Hotfix 20B1: About information icon

- Replaced the thin circled-information text glyph in the About toolbar button
  with Qt's standard information icon at 16 logical pixels, separate from its
  text label.
- Recorded the successful Feature 20B Windows build and `2206/154` regression
  suite, followed by user-confirmed toolbar/About appearance, working MVS/AES
  and Neo Geo CD context benchmarks, and clean close-button behavior.

---

## 2026-09-06 — Feature 20B: toolbar organization and context benchmark

- Grouped Tools, Settings, and About at the right end of the top toolbar.
- Removed the redundant toolbar Exit button and its unused stylesheet rules;
  the window X and existing close-event behavior remain available.
- Kept Random in the left toolbar group and retained its Ctrl+R shortcut.
- Replaced Random in the game context menu with Benchmark, using the existing
  selected-media benchmark flow for the clicked parent, variant, CUE, or CHD.
- Adopted the user-supplied About text and link revision, including the
  `LLM (A.I.) assistance` credit, as the unchanged input to this patch.

---

## 2026-09-02 — Hotfix 20A: About placement and metadata polish

- Moved About Goliath from the Tools menu to a dedicated compact toolbar
  button immediately left of Exit.
- Added a diagnostic stethoscope marker to Diagnostics & Logs without changing
  the action or its logging behavior.
- Corrected the author credit to `epsx`.
- Split build revision and UTC configure date across two centered lines so the
  complete values remain visible inside the identity card.
- Retained the validated Feature 20 layout, links, frameless behavior, and all
  active-theme palettes.

---

## 2026-09-02 — Feature 20: themed About Goliath dialog

- Added a dedicated product About dialog under Tools, separate from the
  installed-component capabilities retained in Settings -> Info.
- Reused the shared frameless title bar and active theme, with a large embedded
  Goliath icon, identity/information cards, version and build metadata, project
  authorship, and upstream acknowledgements.
- Added verified browser actions for Jolly Good, JG, JGRF, Geolith, and Qt,
  plus standard About Qt and accessible Close/X/Escape behavior.
- Kept the future Goliath repository and final-license actions visible but
  safely disabled until their build-time URLs are explicitly configured.
- Added all-theme regression coverage for the About cards and primary Close
  action.

---

## 2026-09-01 — Feature 19C: restored themed input section cards

- Restored the large outer card behind every visual input panel in both
  Settings and per-game Input Mapping.
- Made the custom `InputPanelWidget` explicitly paint Qt's styled-widget
  primitive, allowing the existing theme-aware background, border, and rounded
  corners to render without reintroducing fixed dark colors.
- Preserved every button size, mapping, layout, and input persistence path.
- Added all-theme regression coverage for the complete outer-card style rule.

---

## 2026-09-01 — Feature 19B: global frameless-window separation

- Added a theme-aware one-pixel perimeter to the main window and every custom
  frameless dialog, keeping light palettes visually distinct from similarly
  colored desktop or browser backgrounds.
- Centralized the dialog behavior through `setupFramelessDialog`, covering
  Settings, Game Settings, Save Data Manager, Benchmark, Diagnostics, Rescan,
  and BIOS verification without per-dialog styling.
- Disabled the redundant native status-bar size grip; Goliath's existing
  all-edge resize filter remains active and the stray square beside Launch is
  no longer created.
- Added all-theme regression coverage for the shared perimeter rule.

---

## 2026-09-01 — Hotfix 19A4A: theme delegate lookup lifetime diagnostic

- Bound the popup delegate's current Qt theme name to a local `std::string`
  before catalog lookup, eliminating GCC 16's `-Wdangling-reference`
  diagnostic while preserving the exact 19A4 rendering behavior.

---

## 2026-09-01 — Hotfix 19A4: explicit theme popup text rendering

- Confirmed that direct popup-view styling fixed the dark-theme cascade while
  Qt's Windows item delegate could still repaint light-theme row text from the
  combo button palette.
- Added a dedicated popup delegate that paints normal, hovered, and selected
  rows from the active theme palette, including explicit contrast-aware text.
- Added all-theme regression coverage for the delegate foreground contract and
  its minimum `4.5:1` normal and selected text contrast.

---

## 2026-09-01 — Hotfix 19A3: theme popup cascade precedence

- Confirmed through paired light/dark Windows captures that the 19A2 popup
  selector fixed its surfaces and selection state, but Qt still inherited the
  accent button foreground for unselected rows.
- Moved the popup rules into a dedicated stylesheet installed directly on the
  popup view after the main window theme, fixing the popup surface cascade and
  the affected dark-theme rendering path.
- Expanded all-theme regression coverage for the isolated popup stylesheet,
  its fully resolved palette, and its minimum `4.5:1` text contrast.

---

## 2026-09-01 — Hotfix 19A2: readable light-theme selector popup

- Introduced a dedicated semantic selector for the theme popup and established
  contrast regression coverage for every built-in palette.
- Used each palette's hover surface and primary text for unselected popup
  items, retaining accent-aware contrast for the selected entry.
- Added all-theme regression coverage for the popup selector and a minimum
  `4.5:1` foreground/background contrast.

---

## 2026-09-01 — Hotfix 19A1: theme lookup lifetime diagnostics

- Bound configuration and Qt theme names to local `std::string` values before
  resolving catalog references, eliminating GCC 16 `-Wdangling-reference`
  diagnostics while preserving the exact Feature 19A behavior and palettes.

---

## 2026-09-01 — Feature 19A: light and refreshed theme collection

- Added six complete light palettes: Goliath Pearl, Warm Ivory, Arctic Blue,
  Neo Geo Classic, High Contrast Light, and Sakura Paper.
- Replaced One Dark Pro with Tokyo Night and Monokai Pro with Gruvbox Dark,
  while transparently migrating either legacy configuration key.
- Made selection, success, and danger foregrounds choose black or white from
  WCAG contrast, preserving readable text on both bright and dark accents.
- Removed fixed dark colors from the main details view, informational notes,
  status text, and input-panel containers so every built-in palette reaches
  those surfaces consistently.
- Added regression coverage for palette order and uniqueness, valid colors,
  legacy-key migration, semantic token completion, branch-asset placeholders,
  and a minimum `4.5:1` contrast for colored UI roles.

---

## 2026-08-31 — Cleanup 18H3: selective supplemental INI parsing

- Retained supplemental INI metadata only for IDs reachable from the Neo Geo
  cartridge/CD catalogs or from local `.neo` filename stems, preserving
  metadata for unrecognized homebrew files as well as catalog games.
- Kept the full loader as a reference API and preserved section handling,
  last-value duplicate precedence, category fallbacks, and the independently
  selective History maps.
- Across five read-only runs over `211266` parsed INI rows, reduced the median
  five-file phase from `55.366 ms` to `22.425 ms`; only `1327` relevant rows
  were retained, saving `32.941 ms` (`59.5%`) with exact map equivalence.
- Added focused regression coverage for irrelevant rows, duplicate values,
  folder sections, catalog IDs, local homebrew stems, and unchanged History
  behavior, bringing the target to `1933 assertions / 146 test cases`.

---

## 2026-08-30 — Cleanup 18H2: Redump physical-track layout indexing

- Built one per-scan Redump index keyed by each catalog entry's ordered
  physical-track sizes, replacing the complete DAT walk previously repeated
  for every local CUE.
- Preserved catalog order for duplicate layouts, exact SHA-1 verification,
  title-based ambiguity resolution, cancellation, cache behavior, and all
  generated Neo Geo CD database content.
- Across five read-only runs over `111` CUE images, reduced the median Redump
  phase from `124.101 ms` to `2.593 ms` including index construction
  (`1.172 ms`) and indexed lookup/SHA-1 matching (`1.421 ms`), a conservative
  `121.508 ms` saving with exact per-CUE candidate and match equivalence.
- Added focused regression coverage for ordered layouts, duplicate catalog
  rows, reversed track order, missing layouts, and entries without physical
  tracks, bringing the target to `1897 assertions / 145 test cases`.

---

## 2026-08-30 — Cleanup 18H1: selective Neo Geo History parsing

- Classified each `history.xml` entry by its Neo Geo cartridge, Neo Geo CD,
  or whitelisted machine-only identifiers before normalizing its text, avoiding
  expensive regex work for unrelated MAME systems.
- Preserved catalog separation, first-entry-wins duplicate handling, blank-line
  normalization, machine-only compatibility entries, progress counts, and the
  generated database content.
- Reduced the measured History phase from approximately `1174.636 ms` to
  `181.963 ms` on the full `116692`-entry metadata file while producing
  identical cartridge and CD maps.
- Added focused regression coverage for irrelevant systems, shared cartridge/CD
  identifiers, machine-only entries, duplicate precedence, missing text, and
  progress totals.

---

## 2026-08-29 — Cleanup 18D5A: test executable runtime linkage parity

- Applied the established MinGW static compiler-runtime policy to
  `goliath-qt-tests`, eliminating its dynamic `libgcc_s_seh-1.dll` and
  `libstdc++-6.dll` dependencies while keeping Qt and SDL3 shared.
- Preserved the test executable's Windows console subsystem through the
  existing post-Qt `-mconsole` override, without changing production code,
  runtime behavior, or the `1881 assertions / 143 test cases` baseline.

---

## 2026-08-29 — Cleanup 18D4: persistence regression and recovery validation

- Explicitly disabled `QSaveFile` direct-write fallback for Goliath's own
  configuration, the order-preserving JGRF/Geolith INIs, exact-media profiles,
  playtime statistics, and the generated game database, matching the existing
  SHA-1 cache and Save Data restore policy.
- Extended the cross-platform persistence matrix for INI documents, profiles,
  playtime, database scans, and the SHA-1 cache: an ordinary file blocking the
  destination directory forces a clean failure and remains byte-identical;
  removing it allows the same state or a fresh scan to retry successfully.
- Reloaded every recovered document, including a SHA-1 cache hit from the
  entry kept dirty after its failed save, while preserving all paths, formats,
  schemas, normal write behavior, and UI contracts. The target is now
  `1881 assertions / 143 test cases`.

---

## 2026-08-29 — Cleanup 18D3B: live save-data directory safety

- Required every JGRF-owned component of the active `state` and `save`
  branches to remain a direct directory before Save Data Manager inventories,
  snapshots, restores, or deletes any live Geolith data.
- Refused symbolic-link, junction, and reparse-point substitutions before the
  first mutation, while retaining the configured data/executable directory as
  the platform-appropriate trust anchor.
- Kept restore writes atomic with `QSaveFile`, explicitly disabled its direct
  write fallback, and validated both branches before applying a snapshot so a
  blocked second branch cannot leave a partial restore.
- Added a cross-platform directory-link regression proving that inventory,
  backup, restore, and deletion all refuse an externally redirected live save
  branch and leave both the external sentinel and existing backup untouched,
  bringing the target to `1829 assertions / 141 test cases`.

---

## 2026-08-29 — Cleanup 18D3A: SHA-1 cache atomic target safety

- Replaced the predictable `hash_cache.json.tmp` truncation/rename/copy
  sequence with the same `QSaveFile` transaction model already used by the
  library and profile JSON writers.
- Removed the Windows overwrite-copy fallback, so cache replacement can no
  longer write through an object occupying either the final destination or
  the old fixed temporary name.
- Preserved the cache schema, formatting, hit/invalidation/pruning behavior,
  and harmless save-failure contract: a missing cache is recalculated during
  the next complete scan.
- Added a cross-platform hard-link regression proving that an unrelated file
  aliased at the legacy temporary name remains byte-identical, bringing the
  target to `1810 assertions / 140 test cases`.

---

## 2026-08-28 — Cleanup 18D2B2: direct log target safety

- Added one native direct-file opening policy for runtime logs: Windows opens
  the final entry with reparse-point traversal disabled, while POSIX uses
  no-follow creation/opening where supported; directories, links, junctions,
  reparse points, and other special objects are refused before any write.
- Routed frontend-log initialization, JGRF detached-launch preflight,
  benchmark output append, diagnostics status/open, and JGRF-log clearing
  through the same direct-object policy instead of independent path checks.
- Made an unsafe frontend-log destination visible at startup, propagated
  detached-launch log refusal details to the UI, and retained benchmark
  results while reporting a mid-run append refusal once in the frontend log.
- Preserved normal log names, append order, ANSI bytes, clear behavior,
  detached JGRF lifetime, and benchmark semantics. Added regressions proving
  binary append preservation and that directory/junction substitutions cannot
  start a detached process or modify external contents, bringing the target
  to `1794 assertions / 139 test cases`.

## 2026-08-28 — Cleanup 18D2B1: per-game runtime target safety

- Added direct-object filesystem inspection that treats symbolic links,
  Windows junctions, and other reparse points as substituted objects rather
  than ordinary files or directories.
- Required every Goliath-owned per-game runtime directory component to be a
  direct directory before materializing settings, and refused non-regular INI
  destinations before deleting, copying, or atomically saving them.
- Replaced overwrite-through copy behavior with remove-only-confirmed-file plus
  non-overwriting creation, so an object substituted between those operations
  causes launch preparation to fail instead of being followed.
- Added cross-platform regressions that create a real directory symlink or
  Windows junction at an exact-media runtime path and place a directory where
  an INI belongs, proving both external and same-name contents remain untouched.

---

## 2026-08-28 — Cleanup 18D2A: BIOS destination ownership safety

- Removed the last runtime `remove_all()` path from BIOS preparation. Retargeting
  can now remove only a confirmed symbolic link or Windows reparse point; a
  plain directory is preserved as a non-destructive copy fallback, and a
  regular file or other object at the destination is refused unchanged.
- Made failed stale-link removal a hard stop instead of copying through a link
  that still points somewhere else, and allowed long Windows junction targets
  to be inspected without the legacy `MAX_PATH` buffer limit.
- Refused non-regular files inside a plain fallback directory rather than
  following them during overwrite, while retaining existing source-newer copy
  behavior for ordinary BIOS files.
- Added cross-platform regressions proving that an existing directory keeps
  unrelated user files, a non-directory blocker remains byte-identical, and a
  same-name non-regular child cannot be overwritten through copy fallback.

---

## 2026-08-28 — Cleanup 18C4B: atomic global INI persistence

- Made Goliath's own `goliath.ini` and every order-preserving JGRF/Geolith INI
  save transactional, including first-run creation of the frontend config.
- Preserved every existing INI section, key, ordering, formatting, save call,
  and Settings workflow while preventing failed writes from exposing a
  truncated global configuration.
- Let per-game runtime materialization consume the new save result directly,
  so a failed runtime INI commit stops launch preparation with its real error.
- Added regressions for replacing an existing INI and for preserving an
  ordinary file that blocks creation of the requested destination directory.

## 2026-08-28 — Cleanup 18C4A: atomic library/profile persistence

- Replaced direct truncating writes for `database/games.json` and
  `config/game_profiles.json` with `QSaveFile` transactions. Failed opens,
  short writes, full disks, or failed commits now leave the previous complete
  document in place instead of exposing a partial file.
- Made Rescan report database-directory, open, write, and commit failures as
  errors instead of announcing a database that was never written.
- Added regressions for an unusable database destination and replacement of an
  existing exact-media profile document.
- Validated with `1693 assertions / 131 test cases`, a complete Rescan, the
  expected `279` cartridge files and `221` verified Neo Geo CD images, and all
  four exact-media profile smoke checks.
- Kept database/profile schemas, JSON formatting, scanning and matching logic,
  launch arguments, runtime profile behavior, and UI contracts unchanged.

## 2026-08-28 — Cleanup 18C2: shutdown and cancellation hygiene

- Bounded every asynchronous `jollygood --help` capability probe to three
  seconds, so a configured executable that starts but hangs cannot keep the
  Video, Info, or exact-media Game Settings lifetime open indefinitely.
- Distinguished a killed timeout from a completed probe, preventing partial
  help output from being accepted as a valid Vulkan/version result.
- Audited detached gameplay tracking and rescan shutdown without changing
  their established contracts: JGRF survives Goliath, observer handles are
  released, elapsed playtime is persisted, and scanner workers are stopped and
  joined before destruction.
- Added a cancellation regression proving that an interrupted rescan leaves an
  existing `database/games.json` byte-for-byte untouched.
- Kept launch arguments, persistent formats, normal JGRF lifetime, and the
  validated `1665 assertions / 129 test cases` baseline otherwise unchanged.

## 2026-08-27 — Cleanup 18C1: runtime ownership and worker completion

- Gave the top-level `MainWindow` deterministic automatic lifetime so closing
  the application now runs its QObject/widget destruction chain instead of
  leaving a hidden heap allocation until process termination.
- Made joystick-listener cancellation wait for actual thread completion,
  preventing a timeout from allowing a parent dialog/widget to destroy a
  still-running `QThread`.
- Delivered asynchronous audio-device and Geolith capability results through
  context-bound `QThread::finished` connections. Workers no longer inspect UI
  guard pointers from background threads, and Qt suppresses callbacks
  automatically when their tab has already been destroyed.
- Kept launch arguments, scanner behavior, persistent formats, detached JGRF
  lifetime, and the validated `1665 assertions / 129 test cases` baseline
  unchanged.

## 2026-08-27 — Cleanup 18B2: CMake dependency-scope hygiene

- Disabled compiler-specific C++ language extensions so every target requests
  standard C++20 instead of a GNU-flavored dialect.
- Kept the project's `src` include interface public while making vendored
  headers private system dependencies; tests now declare their own direct
  access to Catch2, JSON, and miniz headers.
- Kept Qt Core public because project headers expose Qt types, while SDL3 and
  the Windows `ole32`/`uuid` libraries are private implementation dependencies.
- Changed build metadata only; runtime behavior, persistent formats, JGRF
  arguments, UI behavior, and the validated
  `1665 assertions / 129 test cases` baseline remain unchanged.

## 2026-08-27 — Cleanup 18B1: header self-containment and include ownership

- Replaced accidental transitive dependencies with direct standard/Qt
  includes or forward declarations, keeping public headers independently
  parseable and reducing unnecessary recompilation fan-out.
- Qualified fixed-width and size types through `std::`, added the headers that
  own `std::move`, character classification, and event declarations, and
  removed one unused standard include.
- Kept runtime behavior, persistent formats, JGRF arguments, UI behavior, and
  the validated `1665 assertions / 129 test cases` baseline unchanged.

## 2026-08-27 — Feature 17: one-shot exact-media audio WAV export

- Added **Tools -> Export Selected Audio WAV...** and the matching library
  context action for cartridge parents, variants/hacks, CUE, and CHD.
- Reused the normal detached launch pipeline so exact-media profiles, optional
  session verbose logging, BIOS preparation, long-path media handling, log
  capture, Windows volume, and playtime tracking remain authoritative.
- Exposed stock JGRF's `--wave <file>` only for the requested launch, before
  the final media argument, without writing global INIs, per-game profiles, or
  scanner data and without affecting Random or Benchmark.
- Added readable timestamped suggestions below
  `data/goliath/audio_exports/`, portable filename sanitization, `.wav`
  normalization, and collision suffixes while preserving user-selected paths.
- Mirrored JGRF's no-overwrite contract: existing files, directories, and
  symlinks are rejected; configured `-o`/`--wave` arguments are treated as a
  conflict; and a removable same-directory write probe reduces the risk of
  JGRF 2.0.1 failing after its internal wave writer cannot open the target.
- Documented that the detached recording survives Goliath shutdown but needs a
  normal JGRF exit to finalize its WAV header.
- Added 50 assertions across portable naming, path safety, collision,
  conflict, preparation, and argument-order coverage, bringing the regression
  target to `1665 assertions / 129 test cases`.

## 2026-08-26 — Hotfix 16B: atomic merged JGRF log capture

- Merged detached JGRF stdout and stderr through Qt before redirecting them to
  `jollygood.log`, replacing two independent append handles to the same file.
- Prevented simultaneous informational and verbose/core writes from
  overwriting or splicing log bytes on Windows while preserving detached
  process lifetime, ANSI output, argument order, and append behavior.
- Kept the validated `1615 assertions / 125 test cases` baseline; runtime
  verification covers one verbose cartridge launch and one long-path CUE.

## 2026-08-26 — Hotfix 16A: platform-aware verbose CUE test

- Corrected the Feature 16 CUE argument expectation to use the established
  platform-specific launch-path preparation. Windows intentionally converts
  every absolute CUE to `\\?\` extended-length form so Geolith can resolve
  derived track paths beyond `MAX_PATH`; other platforms retain the logical
  path.
- Changed regression code only. Runtime launch behavior and the
  `1615 assertions / 125 test cases` baseline remain unchanged.

## 2026-08-26 — Feature 16: per-launch verbose logging and log clear

- Added **Tools -> Diagnostics & Logs...** with a session-only toggle that
  appends JGRF's `--verbose` option to normal Launch and Random operations.
- Kept verbose state out of `goliath.ini`, exact-media profiles, generated
  profile INIs, and the selected-game benchmark path. An explicit `-v` or
  `--verbose` already present in JGRF Arguments remains authoritative and is
  never duplicated.
- Added direct open/status controls for `goliath-qt-debug.log` and
  `jollygood.log`, plus separate confirmation-gated clear operations.
- Made frontend-log clearing preserve `DebugLogger`'s live synchronized file
  handle so later events continue normally. The standalone JGRF-log helper
  treats a missing file as already clear and rejects directories, symbolic
  links, and other non-regular targets.
- Disabled JGRF-log clearing while any process tracked by the current Goliath
  session may still be active, with an explicit warning for external or
  restart-surviving JGRF processes that Goliath cannot safely identify.
- Added verbose-option placement/deduplication, profile integration, missing-
  log idempotence, truncation, and non-regular-target coverage, bringing the
  regression target to `1615 assertions / 125 test cases`.

## 2026-08-26 — Hotfix 15A: save-data table theme hygiene

- Added explicit themed base, alternating-row, hover, selection, inactive-
  selection, grid, and header colors for Qt tables.
- Prevented the Windows/Fusion system palette from producing white unreadable
  rows in the Save Data Manager without changing its data or file operations.
- Kept the validated `1583 assertions / 122 test cases` regression baseline;
  this hotfix changes presentation only.

## 2026-08-26 — Feature 15: safe save-data manager

- Added **Manage Save Data...** for the exact selected cartridge parent,
  variant/hack, CUE, or CHD through both Tools and the library context menu.
- Matched stock JGRF naming and storage behavior, including its 127-byte game
  basename limit, dynamic/static data layouts, state slots `0` and `1`, and
  Geolith `.nv`, `.srm`, `.mcr`, `.brm`, and `.dip` persistent files.
- Added manual exact-media ZIP snapshots below
  `data/goliath/save_backups/<hash>/`, with an identity/file manifest and no
  launch-time or automatic backup growth.
- Made restore reject foreign media, path traversal, duplicate/unsupported or
  encrypted members, undeclared files, invalid sizes, and CRC failures before
  touching live data. Restore and live-file deletion create a protective
  snapshot first; a failed restore attempts an in-memory rollback.
- Limited deletion to the seven computed live filenames or a direct regular
  ZIP in the selected exact-media backup directory. Arbitrary paths, symlinks,
  arbitrary renaming, and unsupported state slots are not exposed.
- Switched the dialog to read-only while any JGRF process tracked by the
  current Goliath session may still be active, and provided explicit state,
  save, and backup folder actions.
- Added upstream-name, layout, inventory, round-trip, exact-media isolation,
  foreign-manifest, ZIP traversal, and protected-deletion coverage, bringing
  the regression target to `1583 assertions / 122 test cases`.

## 2026-08-25 — Feature 14: per-game advanced video overrides

- Reorganized exact-media Game Settings into **General**, **Video**, and
  **Input Mapping** tabs and added a video-only reset that preserves every
  non-video profile choice.
- Added inheritance-aware fullscreen/windowed startup, initial scale, all
  exposed CRTea parameters, Geolith aspect ratio/palette, and four-edge
  overscan masking for every parent, variant/hack, CUE, and CHD.
- Extracted one authoritative JGRF/Geolith video schema shared by global
  Settings, per-game UI, JSON validation, global-value resolution, and runtime
  composition, removing duplicated keys/ranges/options.
- Matched upstream CRTea semantics: Custom-only mask/strength/scanline/
  sharpness controls, with curve/corner/Trinitron tuning available for every
  CRTea preset.
- Layered advanced values into the disposable profile `geolith.ini`; API,
  fullscreen/windowed, scale, and shader retain final CLI precedence where
  supported. Global INIs remain byte-for-byte untouched.
- Preserved additive version-1 profile compatibility and added schema,
  persistence, invalid-value, precedence, isolated-INI, and launch-argument
  coverage, bringing the regression target to
  `1518 assertions / 114 test cases`.

## 2026-08-25 — Feature 13: search clear UX

- Enabled Qt's native trailing clear button in the library search field. It is
  visible only while a filter contains text and restores the complete list
  through the existing live-filter path.
- Added an Escape shortcut scoped strictly to the focused search field, while
  preserving the existing Ctrl+F focus/select-all behavior and all library
  selection/filter state.
- Kept the validated `1462 assertions / 113 test cases` regression baseline;
  this feature changes only main-window search UX.

## 2026-08-25 — Feature 12: exact-media playtime tracking

- Added persistent playtime, tracked-session count, and last-played time for
  every exact cartridge parent, variant/hack, CUE, and CHD.
- Observed normal detached JGRF launches without changing or terminating them.
  Windows retains a stable process handle and Linux uses a `pidfd` when
  available, avoiding numeric-PID reuse during long sessions.
- Replaced the prototype's thread-per-launch model with one GUI timer that can
  supervise multiple simultaneous games without worker shutdown waits.
- Stored statistics atomically in `config/game_playtime.json`, outside
  scanner-generated `games.json`, with strict schema/range validation and
  saturating counters.
- Ignored processes shorter than five seconds as probable launch failures.
  Benchmark runs remain excluded. If Goliath closes while a detached game is
  active, it stores only the observed portion through launcher shutdown.
- Added persistence, malformed-record, exact-key, formatting, threshold,
  failure-path, and overflow coverage, bringing the regression target to
  `1462 assertions / 113 test cases`.

## 2026-08-24 — Feature 11: per-game video API override

- Added **Video API** to exact-media Game Settings, with inheritance plus
  OpenGL Core, OpenGL ES, OpenGL Compatibility, and capability-gated Vulkan.
- Probed the configured JGRF executable asynchronously before exposing a new
  Vulkan override. Existing Vulkan profiles are preserved and identified when
  the current executable cannot advertise the renderer.
- Stored the optional `video_api` field in `config/game_profiles.json` without
  changing the additive version-1 profile schema or any global INI.
- Applied valid per-game APIs through JGRF `--video` after global arguments,
  preserving media-last ordering, CUE long-path handling, normal detached
  launches, and managed benchmarks.
- Updated Benchmark result resolution so the displayed renderer reflects the
  exact-media override already present in the prepared command line.
- Added persistence, invalid-value, argument-order, and launch-environment
  coverage, bringing the regression target to
  `1400 assertions / 104 test cases`.

## 2026-08-24 — Feature 10: selected-game performance benchmark

- Added **Tools -> Benchmark Selected Game...** for an exact cartridge parent,
  variant/hack, CUE, or CHD.
- Reused the normal launch preflight, CHD capability gate, long-path policy,
  BIOS preparation, and optional exact-media profile without persisting a
  benchmark flag or modifying global/per-game settings.
- Added Quick, Standard, Extended, and custom frame counts. The JGRF process is
  supervised rather than detached so the dialog can validate completion,
  cancel safely, and keep captured output in `jollygood.log`.
- Reported total process time, effective frames/second, average milliseconds
  per frame, video API, shader, and effective configuration source, with a
  Copy Result action.
- Documented that stock JGRF Benchmark Mode bypasses per-frame audio sample
  processing, requests OpenGL VSync off and immediate Vulkan presentation when
  available, and still renders every frame. Results therefore measure
  frontend/core/render throughput rather than normal gameplay smoothness or a
  pure core loop.
- Added deterministic argument-placement, completion-parser, option-resolution,
  and metric tests, bringing the regression target to
  `1387 assertions / 103 test cases`.

## 2026-08-24 — Feature 9: per-game input mapping

- Added an **Input Mapping** tab to exact-media Game Settings for independent
  parent, variant/hack, CUE, and CHD keyboard/controller mappings.
- Reused the existing visual Joystick, Mahjong, 4-Player, V-Liner, Irritating
  Maze, and System panels without duplicating the global Settings editor.
- Preserved inheritance per control, with custom-map markers, individual
  right-click reset, displayed-profile reset, and whole-profile reset.
- Layered mappings after the optional inherited Player 1 port retarget, so an
  explicit captured `jN...` binding retains its literal connection-order port.
- Added strict JGRF 2.0.1 bounds validation for keyboard, mouse, joystick axis,
  button, and hat codes; SDL capture now observes the same six-axis/32-button
  limits as JGRF's fixed arrays.
- Added safe built-in input defaults when no global input INI exists, while
  keeping all global JGRF/Geolith files unmodified.
- Added persistence, malformed-input, runtime-layering, default-inheritance,
  and binding-boundary coverage, bringing the regression target to
  `1360 assertions / 99 test cases`.

## 2026-08-24 — Feature 8B: open per-game config folder

- Added **Open per-game config folder** to the library context menu for rows
  with an active exact-media profile.
- Reused the existing runtime composer to refresh the selected parent,
  variant/hack, CUE, or CHD configuration before opening its generated
  `config/p/<hash>/jollygood/` directory.
- Kept hash construction private and preserved the disposable runtime model;
  global JGRF/Geolith INIs remain unmodified.
- Retained `1320 assertions / 96 test cases` as the regression target; manual
  validation covers context-menu visibility and exact-media folder selection.

## 2026-08-24 — Hotfix 8A1: keyboard-capture UX and lifecycle

- Clarified that keyboard mapping is always active and that the selected SDL
  controller is captured alongside it.
- Added click-again cancellation without reserving Escape, which remains a
  valid JGRF keyboard binding.
- Centralized capture cleanup across tab, input-profile, controller, Refresh,
  Save, Reset, close, and destruction paths. Keyboard mappings now also stop
  the parallel joystick listener immediately.
- Changed controller labels to the explicit `Port 1 (j0)` convention and added
  a keyboard-only state when no SDL controller is connected.
- Retained `1320 assertions / 96 test cases` as the regression target; this
  hotfix changes only Settings capture UX and its documentation.

## 2026-08-23 — Feature 8A: per-game launch profiles

- Added persistent profiles keyed by system plus exact relative media path, so
  every MVS/AES parent, variant/hack, CUE, and CHD can differ independently.
- Added per-media cartridge/CD BIOS, Universe BIOS hardware, region, emulated
  input, shader, MVS Free Play/Setting Mode, and Player 1 controller-port
  overrides. Every field can inherit the current global setting.
- Kept profiles outside generated `games.json` in
  `config/game_profiles.json`, preserving them across rescans.
- Added an isolated JGRF configuration composer below `config/p/`. Profiled
  launches receive their own `XDG_CONFIG_HOME`, while the normal data tree and
  global INIs remain shared/unmodified.
- Enforced MVS-only Free Play/Setting Mode, direct-MVS JP/AS four-player mode,
  safe CD input fallback, and JGRF's 128-byte configuration-path boundary.
- Added controller-port retargeting for inherited Player 1/single-player
  joystick mappings without altering keyboard, Player 2+, or unrelated input
  sections. The UI documents JGRF's connection-order limitation.
- Added **Game Settings** to Tools and the library context menu, plus a gear
  marker for active profiles and **Reset to Global** removal.
- Added seven focused profile tests plus launch argument/environment coverage,
  bringing the regression target to `1320 assertions / 96 test cases`.

## 2026-08-23 — Cleanup 7F validation and launch/verification hotfixes

- Completed a fresh Release configure/build, the `1189/85` suite, subsystem and
  dependency inspection, checksums, and MVS/AES/CUE/CHD/Settings/Random smoke
  tests.
- **7F1** fixed MinGW static winpthread link ordering without changing the Qt
  and SDL3 shared-runtime deployment model.
- **7F2** added derived Windows long-path handling for CUE track paths; the
  suite reached `1194/85` and both reported long CUE variants launched.
- **7F3** replaced track-only Redump acceptance with complete CUE/BIN set
  verification and an explicit CUE-mismatch state/tooltip; the suite reached
  `1235/87`.
- **7F4** corrected Neo Geo CD Redump/MAME alias mapping, including Last Blade
  2, and established the validated `1239/88` baseline.

---

## 2026-08-23 — Cleanup 7: final legacy and repository audit

Cleanup 7 was intentionally split into focused, independently verified patches.
It did not introduce new emulator functionality.

### Cleanup 7A — Source comment and naming hygiene

- Replaced the remaining Romanian production comments in the integrated
  scanner/rescan path with concise English descriptions.
- Removed obsolete wording that described the scanner as a separate
  `goliath-db` process.
- Updated the scanner, model, worker, and rescan-dialog headers to describe
  their current responsibilities and contracts.
- Removed historical migration terminology from production-facing comments
  while retaining stage names in this history where they remain useful.
- Added the missing final newline to `db_scanner.cpp`.

Affected files were limited to the scanner/model/rescan documentation surface.
The application built successfully and the full `1189/85` suite passed.

### Cleanup 7B — Source formatting hygiene

- Corrected remaining tab/space inconsistencies in `db_scanner.cpp` and scanner
  tests.
- Reindented `game_model.cpp` without changing validation order or behavior.
- Removed isolated trailing whitespace and redundant blank lines from joystick
  and library code.
- Reformatted scanner-test assertions for consistent nesting and readability.

The application and tests built successfully; the baseline remained
`1189 assertions / 85 test cases`.

### Cleanup 7C — Repository line-ending hygiene

- Added `.gitattributes` with explicit LF rules for first-party CMake, Markdown,
  source, test, and resource text.
- Marked ICO and ZIP assets as binary.
- Normalized `tests/test_game_model.cpp` from CRLF to LF without changing its
  C++ content.

The mixed-line-ending input required `patch --binary` under MSYS2 for reliable
application. The rebuilt tests passed at `1189/85`.

### Cleanup 7D — Repository artifact and orphan hygiene

- Added `.gitignore` rules for CMake/Ninja output, binaries, runtime state,
  generated databases, user media, metadata, patches, archives, backups, and
  operating-system artifacts.
- Added `.gitignore` itself to the LF policy.
- Removed the stale `assets/gen_ico.py` helper after confirming that its
  four-size generated ICO did not reproduce the authoritative ten-image
  `assets/goliath-qt.ico`.
- Kept the canonical ICO, QRC, and Windows RC assets intact.
- Kept external `metadata/` untracked while preserving normal runtime scanner
  access to it.

No compiled target depended on these repository-only changes, so Ninja
correctly reported no work. The full test suite still passed.

### Cleanup 7E — Final documentation

Repository documentation was audited against the post-cleanup source and
rewritten in English, one document per patch:

- **7E1 — `README.md`**: current purpose, features, architecture, metadata,
  build, deployment, limitations, and authoritative validation baseline.
- **7E2 — `HOWTO.md`**: practical build, runtime setup, JGRF/Geolith, BIOS,
  rescan, CUE/CHD verification, deployment, troubleshooting, and validation.
- **7E3 — `STRUCTURA.md`**: complete source inventory, CMake target ownership,
  component boundaries, runtime/generated files, and data/control flows.
- **7E4 — `HISTORY.md`**: this consolidated chronological record, with old
  checkpoints separated from the current baseline.

Documentation patches do not modify compiled behavior and therefore do not
require incremental rebuilds.

### Next checkpoint

Cleanup **7F — Final Validation** remains deliberately pending after this
documentation patch. Its scope is a fresh configure/build, full `1189/85`
suite, manual smoke tests, and final repository inventory.

---

## 2026-08-23 — Cleanup 6: build architecture and CMake hygiene

### Cleanup 6A — Shared core build target

Added the internal static library:

```cmake
add_library(goliath-core STATIC ...)
```

Twenty-two production/third-party translation units that were previously
compiled separately for the application and tests are now compiled once and
shared by:

```text
                 goliath-core
                /            \
               v              v
        goliath-qt       goliath-qt-tests
```

The clean app-plus-tests build dropped from roughly 76 C/C++ compile entries to
roughly 54, a reduction of about 29 percent.

Validation:

- `1189 assertions / 85 test cases` passed;
- one MVS/AES game launched;
- one Neo Geo CD CUE launched;
- one Neo Geo CD CHD launched.

### Cleanup 6B — CMake hygiene

- Removed dead AUTOUIC configuration after confirming that the repository has
  no `.ui` files.
- Preserved AUTOMOC for Qt objects and AUTORCC for the application icon.
- Added one `goliath_enable_warnings()` helper and applied the same warning
  policy to core, application, and tests.
- Moved the miniz `-Wno-type-limits` source-specific suppression beside
  `goliath-core`, which owns those C translation units.
- Attached Windows `ole32` and `uuid` requirements to `goliath-core` so its
  consumers inherit the libraries required by `audio_devices.cpp`.
- Removed redundant application/test source include directories plus direct
  SDL3 and Windows system-library declarations supplied by the core target.
- Kept `Qt6::Widgets` on the application and explicit `Qt6::Core` on tests for
  the established MinGW console-link ordering.
- Normalized indentation and block structure in the test target.
- Preserved the application MinGW static libgcc/libstdc++/winpthread behavior.
- Preserved the deliberately ordered MinGW `-mconsole` test workaround and the
  GUI-versus-console `WIN32_EXECUTABLE` distinction.

The incremental build and then a complete clean rebuild produced both
executables. The full test suite passed, followed by manual MVS/AES, CUE, CHD,
Settings, and Random checks.

Cleanup 6 was then considered complete.

---

## 2026-08-23 — Cleanup 1–5: source responsibility audit

### Cleanup 1 — Dead code and initial repository hygiene

- Removed an orphan joystick diagnostic source, a stale `.orig` test file,
  unused members/helpers, dead path-picker branches, redundant path state, and
  obsolete scanner wrappers.
- Consolidated repeated input-panel painting code.
- Audited compatibility and migration code before removal instead of treating
  all old-looking branches as dead.
- Validated the initial cleanup at the then-current Stage 3D baseline of
  `1155 assertions / 82 test cases`.

#### Cleanup 1.1 — Settings save popups

Removed success-only Settings confirmation dialogs while preserving warnings,
questions, errors, and the useful Reset-to-default confirmation.

### Cleanup 2 — Local duplication and permanent naming

- Unified remaining local input-panel drawing behavior.
- Replaced temporary stage-prefixed production identifiers:

```text
stage3d2_media_is_launchable            -> media_is_launchable
stage3d3_prepare_media_path_for_launch  -> prepare_media_path_for_launch
```

- Kept historical stage labels in documentation only.
- Simplified the Tools menu to global operations such as Rescan ROMs and Verify
  BIOS; contextual Random/folder/variant actions stayed in the toolbar or
  right-click menu.

### Cleanup 3 — Path and JGRF resolver consolidation

- Added `jollygood_executable.hpp/.cpp` as the single resolver used by launch,
  capability, Video, Info, and BIOS paths.
- Removed duplicated `.exe` and executable-location logic.
- Added resolver coverage, moving the suite to `1159 assertions / 83 tests`.

### Cleanup 4 — Scanner decomposition

The integrated scanner was separated into focused core modules:

- **4A — `sha1_cache`**: persistent CUE track hash cache, fingerprints,
  normalization, serialization, and pruning.
- **4B — `neocd_verification`**: strict Redump CUE and MAME CHD verification.
- **4C — `neogeo_metadata`**: MAME software lists, Redump DAT, title matching,
  and CD hash indexes.
- **4D — `filesystem_io`**: recursive CD discovery and Windows long-path I/O.
- **4E — supplemental metadata/scanner hygiene**: moved `catver`, `catlist`,
  genre, players, series, history, and compatibility loading out of the scanner
  coordinator.

`db_scanner.cpp` retained orchestration, game assembly, statistics,
cancellation, and JSON output. The goal was responsibility separation, not a
behavioral rewrite.

### Cleanup 5 — MainWindow, launch, and library decomposition

#### Cleanup 5A — BIOS integration and MainWindow hygiene

- Added `jollygood_bios.hpp/.cpp` for junction/symlink/copy preparation and
  legacy JGRF data-directory migration.
- Removed Developer and Publisher rows from the visual details panel because
  the available catalogs always displayed `-`.
- Preserved Developer and Publisher in `Game` and JSON for compatibility.

#### Bugfix 5A.1 — Canonical parent metadata

Fixed clone-before-parent scan order. When the actual parent ROM is encountered,
the top-level group now adopts canonical parent metadata instead of keeping the
first alphabetically discovered clone's data.

The Shock Troopers/Lansquenet 2004 regression received a dedicated test. The
suite moved to `1180 assertions / 84 tests`.

#### Cleanup 5B — JGRF launch pipeline extraction

- Moved non-interactive executable/media/capability/environment/argument
  preparation into `jollygood_launch`.
- Shared `selected_launch_media()` between parent and explicit variant paths.
- Added selected-media regression coverage.

The suite reached the then-current `1189 assertions / 85 tests` baseline.

#### Cleanup 5C — Audio launch/preflight extraction

Added `jollygood_audio.hpp/.cpp` and moved the no-output warning plus Windows
PID/session-volume retry out of `MainWindow`. The baseline stayed `1189/85`.

#### Cleanup 5D — MainWindow UI/theme split

- Added `main_window_ui.cpp` for UI construction, theme application, and branch
  assets.
- Removed redundant theme/UI state and duplicated toolbar styling.
- Fixed required complete-type includes for `QStatusBar` after the split.
- Removed Manufacturer, Short Name, and Genre from Sort.
- Added explicit Name and Year directions.
- Fixed mixed icon/text tree-row overlap with non-uniform row heights.

Genre remained in metadata/details for a future filter/category design.

#### Cleanup 5E — Library browser split and view-state polish

- Added `main_window_library.cpp` for population, sorting, filtering, selection,
  details, variants, snapshots, Random, context actions, folder reveal, and
  status presentation.
- Fixed the required `QPushButton` include after the split.
- Kept active Search applied across sorting, Show Variants, and rescans.
- Limited Random to visible top-level games so filtered-out entries cannot be
  launched accidentally.
- Preserved selection sensibly across view rebuilds.

Cleanup 5 completed with the `1189/85` baseline intact.

---

## 2026-08-22 — Neo Geo CD stages 3A–3D

### Stage 3A — Library/system model

- Added the Neo Geo MVS/AES versus Neo Geo CD library selector.
- Added a separately configurable Neo Geo CD root.
- Added `system` to the `Game`/JSON contract while preserving the legacy
  cartridge default.

### Stage 3B — Recursive CD discovery and metadata

- Added recursive `.cue` and `.chd` discovery.
- Treated a CUE and all referenced tracks as one game.
- Prevented `.bin`, `.iso`, `.wav`, ZIP, and other auxiliary files from
  appearing as separate library entries.
- Associated CD title metadata through `neocd.xml`.
- Kept paths relative to the user-configured CD root.

### Stage 3C — Trusted content verification

- Added Redump CUE verification using exact track byte sizes and SHA-1 values.
- Added MAME CHD verification using the CHD v4/v5 internal combined SHA-1 from
  `neocd.xml`.
- Added `✓ Redump CUE` and `✓ MAME CHD` badges.
- Kept metadata-only and Unknown states explicit instead of claiming false
  content verification.
- Added `database/hash_cache.json`, cache hit/calculated statistics, safe
  invalidation, and conservative pruning.
- Added traversal protection and case-only CUE track recovery.
- Added Windows long-path scan/verification support without storing `\\?\`
  prefixes in UI, JSON, or cache.

Real mixed-library validation:

```text
Neo Geo CD images       226
Redump CUE verified     116
MAME CHD verified       110
Unknown                   0
Hash cache entries     3417
Warm-scan cache hits   3417
Warm-scan calculated      0
```

CUE and CHD copies of the same title intentionally remained separate top-level
entries.

### Stage 3D — Launch and core integration

- **3D1**: launched CUE through the existing JGRF pipeline with
  `-e neogeocd`, retaining BIOS, audio, renderer, environment, and logging.
- **3D2**: queried the installed Geolith JG ABI; CHD became launchable only when
  `jg_get_systemlist()` advertised `chd` for `neogeocd`.
- Added Settings -> Info values for JGRF, Geolith, JG, CD formats, CHD, and
  Vulkan capabilities.
- Confirmed stock Geolith CHD support without source patches:

```bash
make clean
make -j$(nproc) ENABLE_CHDR=1 CPPFLAGS="-DHAVE_CHDR"
```

- **3D3**: converted only the final external CD argument to Windows
  extended-length form. A CUE path longer than 300 characters launched with all
  28 referenced tracks.
- **3D4**: audited `cdsystem` and `cd_dma_len_limit` from Settings persistence
  through `geolith.ini` to Geolith runtime consumption.

BIOS behavior confirmed during the audit:

- Front/Top Loader uses `neocd.zip` plus auxiliary `000-lo.lo` from
  `neocdz.zip`;
- CDZ/Universe BIOS uses `neocdz.zip`;
- `cdsystem` defaults to CDZ (`2`);
- `cd_dma_len_limit` defaults to `0`.

The architecture continued to use stock/upstream JGRF, JG, and Geolith.

Historical Stage 3D baseline:

```text
All tests passed (1155 assertions in 82 test cases)
```

---

## 2026-08-16 — Joystick axis-direction fix

- Preserved physical axis direction in digital mappings as
  `j<port>a<axis>+` or `j<port>a<axis>-`.
- Fixed arcade controls that report opposite digital directions through one
  analog axis.
- Correctly detected `-1 <-> +1` transitions.
- Verified buttons, hats, and axes with a PS4 controller and HORI arcade stick.
- Added positive/negative direction tests.

Historical checkpoint: `30 test cases / 172 assertions`.

---

## 2026-08-15 — Game model and details-panel stabilization

- Stabilized the `Game`/JSON contract and rejected invalid database rows.
- Made `main_rom` the authoritative parent launch file and validated its target.
- Normalized `history.xml` and restricted history association to Neo Geo data.
- Kept Developer and Publisher as distinct model fields at this stage.
- Separated metadata from selected-variant state in the details panel.
- Added the Selected Variant display.
- Stabilized snapshot sizing and missing-image placeholders.
- Added responsive details collapse below 1000 px and restore from 1150 px,
  preserving the previous splitter ratio.
- Expanded scanner/model regression coverage.

Historical checkpoint: `27 test cases / 166 assertions`.

---

## 2026-08-11 — Runtime, UI, Settings, and scanner stabilization

### Scanner and compiler hygiene

- Replaced linear description lookup with a precomputed map.
- Added hash-map reserves to metadata/scanner paths.
- Used `std::error_code` for expected filesystem failures.
- Added acquire/release ordering to scanner cancellation.
- Built with `-Wall -Wextra`, fixed incomplete field initialization, and kept a
  source-specific miniz warning suppression.
- Replaced local prefix/suffix helpers with C++20 string methods.

### Settings and renderer support

- Added the Misc tab for stock JGRF `[misc]` settings.
- Added Vulkan (`api=3`) after auditing JGRF source and documentation.
- Built and tested JGRF with `ENABLE_VULKAN=1`.
- Established that Vulkan deployments require the matching `shaders/*.spv`
  files in addition to `jollygood.exe` and the Geolith core.

### Window and details UI

- Generalized the custom title bar for the main window and modal dialogs.
- Added native system resize from every frameless-window edge/corner.
- Reorganized game information beside the snapshot and compacted spacing.
- Reused `setupFramelessDialog()` for Settings, Rescan, and Verify BIOS.

### BIOS path integration

- Added a configurable BIOS folder.
- Moved JGRF data under `<base>/data/jollygood/` to avoid the Linux collision
  between the `jollygood` executable name and a data directory.
- Added one-time legacy data migration plus symlink/junction/copy fallback.
- Fixed custom Windows BIOS junction retargeting without using
  `std::filesystem::remove_all` on a reparse point.
- Accounted for JGRF_STATIC Windows builds that use `<jollygood_dir>/bios`.
- Added non-blocking missing-BIOS warnings and detailed Verify BIOS output.

---

## 2026-08-03 — Rescan, path, and keyboard-capture fixes

- Renamed the local test/deployment folder from `Geolith/` to `Goliath/`.
- Fixed rescan summary detection for multiline output beginning with a newline.
- Added real scanner cancellation and safe worker shutdown when the dialog
  closes.
- Removed noisy per-ROM scanner output in favor of the final summary.
- Recomputed resolved paths after Settings/rescan so changes apply in the same
  session.
- Captured arrow, Tab, and other focus-navigation keys during Input mapping by
  grabbing the keyboard while a control is listening.
- Clarified MVS-only Core labels for Free Play and Setting Mode.

---

## 2026-07-28 — SDL3 input and Path configuration

### SDL3 joystick integration

- Replaced WinMM/direct Linux joystick capture with SDL3 to follow JGRF's own
  connection-order port model.
- Added `sdl_joystick.hpp/.cpp`, a physical Controller selector, and Refresh.
- Generated mappings with the selected `j<port>` instead of hardcoding `j0`.
- Added SDL hat-switch/D-pad capture as `j<port>h0<direction>`.
- Added Input Reset to Defaults scoped to the selected emulated device.
- Recreated the listener for each capture and shut down old threads safely.

### Path and metadata behavior

- Simplified the Path tab to user-facing content locations.
- Added an explicit Save Path Settings action.
- Replaced the old restart instruction with Rescan ROMs/F5.
- Derived the historical `mame.xml` location from the configured metadata root,
  removing the redundant `mame_xml` configuration key.
- Refreshed cached runtime paths after a successful rescan.

---

## 2026-07-24 — C++/Qt project foundations

- Reorganized sources into `common`, `game`, `ini`, `input`, `ui/tabs`, and
  `ui/widgets` domains.
- Introduced an out-of-source CMake/Ninja workflow.
- Added the Goliath application icon through Qt and Windows resources.
- Rebuilt visual input panels with Qt group/grid layouts and fixed Settings
  construction/destruction crashes.
- Added thread-safe `goliath-qt-debug.log` startup, path, launch, environment,
  and error logging.
- Replaced the old JGRF process thread with detached `QProcess` launch while
  preserving combined output in `jollygood.log`.
- Added real stop flags and safe shutdown for input/rescan workers.
- Added the vendored Catch2 test target.
- Extracted testable input normalization from Settings.
- Added initial INI, BIOS, and input-map regression coverage.

---

## Validation milestone table

| Milestone | Assertions | Test cases |
| --- | ---: | ---: |
| Stage 3D before cleanup | 1155 | 82 |
| Cleanup 3 resolver coverage | 1159 | 83 |
| Bugfix 5A.1 parent metadata | 1180 | 84 |
| Cleanup 5B selected-media coverage | 1189 | 85 |
| Cleanup 5C–5E | 1189 | 85 |
| Cleanup 6A–6B | 1189 | 85 |
| Cleanup 7A–7D | 1189 | 85 |
| Feature 14 advanced video overrides | 1518 | 114 |
| Feature 15 safe save-data manager | 1583 | 122 |
| Feature 16 verbose logging and log clear | 1615 | 125 |
| Feature 17 one-shot audio WAV export | 1665 | 129 |

Expected negative model tests print diagnostics for rejected systems, sources,
game rows, ROM rows, and `main_rom` references. Those messages are not failures
when the suite ends at the authoritative baseline.

---

## Current known limitations and future work

These items were intentionally kept outside the cleanup/documentation patches:

- OpenGL ES in the tested JGRF 2.0.1 build fails at startup with a missing
  `glGenBuffers()` entry point; other renderer APIs work.
- Known Geolith BIOS/system errors could be converted from emulator stderr into
  friendlier frontend dialogs.
- Some variant relationship labels remain technically correct but visually
  awkward.
- Genre is a candidate for a real filter/category control, not another sort
  mode.
- Important Reset confirmations can still produce a low-priority transient
  frameless-dialog flash.
- External metadata redistribution and the final project license require a
  dedicated dependency/catalog license audit.
- Final Windows packaging must be validated from the actual Qt, SDL3, JGRF,
  Geolith, and shader dependencies being distributed.

The immediate next project step at this history checkpoint is
**Cleanup 7F — Final Validation**.
