# Goliath Qt HOWTO

This guide covers building, testing, configuring, deploying, rescanning, and
troubleshooting `goliath-qt-cpp`.

See `README.md` for the project overview, `STRUCTURA.md` for the detailed source
map, and `HISTORY.md` for the development history.

---

## 1. Quick start

Run the following commands from the source repository root:

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
```

The normal Windows output is:

```text
build/goliath-qt.exe
```

On Linux, the application is produced without the `.exe` suffix. Tests use a
separate opt-in build described in section 4.

Before running the frontend with real games, prepare a runtime directory with
JGRF and Geolith, then add your own lawfully obtained BIOS files, ROMs, Neo Geo
CD images, and external metadata catalogs as described below. None of that
user-supplied content is distributed with Goliath. Goliath creates
`goliath.ini` next to its executable on first start.

---

## 2. Build requirements

Goliath requires:

- CMake 3.16 or newer;
- a C++20 compiler;
- Qt6 Widgets;
- SDL3;
- Ninja for the documented build workflow.

JGRF, JG, and Geolith are runtime components. They are not built by Goliath's
CMake project.

### Windows: MSYS2 UCRT64

The primary development environment is the **MSYS2 UCRT64** shell.

```bash
pacman -S \
  mingw-w64-ucrt-x86_64-gcc \
  mingw-w64-ucrt-x86_64-cmake \
  mingw-w64-ucrt-x86_64-qt6-base \
  mingw-w64-ucrt-x86_64-sdl3 \
  mingw-w64-ucrt-x86_64-ninja
```

Configure and build from the UCRT64 shell so CMake resolves one consistent
compiler, Qt, SDL3, and Ninja toolchain.

### Linux

Package names vary by distribution. A typical Debian-family setup is:

```bash
sudo apt install \
  build-essential \
  cmake \
  ninja-build \
  qt6-base-dev \
  libsdl3-dev
```

---

## 3. Configure and build

### First configure

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
```

Tests are disabled by default so a normal user build produces only the
application. Developers and CI builds enable the regression target explicitly:

```bash
cmake -S . -B build-tests -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_TESTS=ON
```

### Incremental build

```bash
cmake --build build -j$(nproc)
```

CMake automatically regenerates Ninja files when `CMakeLists.txt` changes.
Normal source changes do not require a manual reconfigure.

### Clean rebuild with the existing configuration

```bash
cmake --build build --target clean
cmake --build build -j$(nproc)
```

This recompiles all targets while preserving the current CMake cache.

### Fresh configure

Use a new, previously unused build directory after moving or renaming the
source tree, changing toolchains, or investigating a stale CMake cache. The
`build-validation/` name below is only an example:

```bash
cmake -S . -B build-validation -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_TESTS=ON
cmake --build build-validation -j$(nproc)
./build-validation/goliath-qt-tests
```

The separate `build-validation/` directory leaves the normal `build/`
directory untouched. If a fresh build succeeds, the old build directory can be
removed after confirming that it is the intended directory.

---

## 4. Run the regression suite

The test executable exists only in a build configured with
`-DBUILD_TESTS=ON`. For an isolated test build:

```bash
cmake -S . -B build-tests -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_TESTS=ON
cmake --build build-tests -j$(nproc)
```

Run the Catch2 executable directly:

```bash
./build-tests/goliath-qt-tests
```

or use CTest:

```bash
ctest --test-dir build-tests --output-on-failure
```

`goliath-qt-tests` (`goliath-qt-tests.exe` on Windows) is for validation only
and must not be copied into a normal user package.

Several negative tests intentionally print diagnostics such as rejected
systems, invalid game rows, invalid ROM rows, or invalid `main_rom` references.
Those lines are expected when the final result still reports that every test
passed.

After any logic change, run the complete suite. UI-only changes still require
the full suite before deployment.

---

## 5. Prepare the runtime directory

All default relative paths are resolved from the directory containing the
Goliath executable, not from the shell's current working directory.

A typical Windows runtime layout is:

```text
Goliath/
├── goliath-qt.exe
├── jollygood.exe
├── cores/
│   └── geolith/
│       └── geolith.dll
├── shaders/                 JGRF Vulkan assets, when enabled
├── bios/
├── roms/
├── neocd/
├── icons/
├── snaps/
├── metadata/                external scanner input
├── database/                generated games.json and hash_cache.json
├── config/
│   ├── game_profiles.json   Goliath per-game overrides
│   ├── game_playtime.json   Goliath exact-media playtime statistics
│   ├── jollygood/
│   │   ├── settings.ini
│   │   ├── geolith.ini
│   │   └── geolith_input.ini
│   └── p/                   generated isolated profile configurations
└── data/
    ├── jollygood/           dynamic-core JGRF runtime data
    └── goliath/
        ├── audio_exports/   suggested one-shot JGRF WAV destination
        └── save_backups/    Goliath exact-media manual ZIP snapshots
```

JGRF's own build may contain additional files and directories. Preserve its
upstream runtime layout instead of copying only the executable and core.

On first start, Goliath creates these files next to `goliath-qt.exe` as needed:

```text
goliath.ini
goliath-qt-debug.log
jollygood.log                created/appended when JGRF is launched
```

The default `goliath.ini` path values are:

```ini
[Paths]
bios = bios
config = config
database = database
icons = icons
jollygood = jollygood
jollygood_args = -c geolith
metadata = metadata
neocd = neocd
roms = roms
snaps = snaps
```

The Path tab exposes the user-facing ROM, CD, icon, snapshot, metadata, and BIOS
directories. Advanced values such as `database`, `config`, `jollygood`, and
`jollygood_args` can be edited carefully in `goliath.ini` while Goliath is not
running.

---

## 6. Install the external metadata

The scanner reads metadata from the configured **Settings -> Path -> Metadata
Folder**. The default is `metadata/` beside the executable.

The complete input set used by the current scanner is:

```text
metadata/
├── neogeo.xml
├── neocd.xml
├── <Redump Neo Geo CD catalog>.dat
├── catver.ini
├── catlist.ini
├── genre.ini
├── nplayers.ini
├── series.ini
└── history.xml
```

Their roles are:

- `neogeo.xml` identifies MVS/AES software and parent/clone relationships;
- `neocd.xml` supplies Neo Geo CD metadata and MAME CHD disk SHA-1 values;
- a valid Redump Neo Geo CD `.dat` supplies CUE track sizes and SHA-1 values;
- the INI files add category, genre, player-count, and series data;
- `history.xml` supplies the history shown in the details panel.

The scanner examines `.dat` files and accepts a catalog only when its XML
header identifies the expected Redump Neo Geo CD data. The filename may include
a catalog version and date.

These catalogs are external runtime input and are intentionally ignored by
Git. They come from multiple projects and must not be redistributed until each
catalog's license and attribution requirements have been reviewed. Keeping
`metadata/` untracked does not prevent Goliath from reading it.

The scanner writes, rather than reads from the repository:

```text
database/games.json
database/hash_cache.json
```

---

## 7. Build and install JGRF and Geolith

Goliath targets stock/upstream JGRF, JG, and Geolith. No private source patch is
required.

### JGRF with Vulkan

From the JGRF source tree, build the Vulkan renderer with:

```bash
make ENABLE_VULKAN=1
```

Deploy the resulting executable together with the generated
`shaders/*.spv` assets. A Vulkan-enabled executable without its matching shader
files may fail at startup.

### Geolith with CHD

From the Geolith source tree, use both CHD build flags:

```bash
make clean
make -j$(nproc) ENABLE_CHDR=1 CPPFLAGS="-DHAVE_CHDR"
```

The flags have separate purposes:

- `ENABLE_CHDR=1` builds and links libchdr support;
- `CPPFLAGS="-DHAVE_CHDR"` makes the JG glue advertise
  `neogeocd -> cue,chd` through `jg_get_systemlist()`.

A core can contain CHD code but still advertise only CUE if the second flag was
omitted. Goliath deliberately blocks CHD launch in that case.

Install the core at JGRF's local core path:

```text
<jollygood directory>/cores/geolith/geolith.dll    Windows
<jollygood directory>/cores/geolith/geolith.so     Linux
```

Open **Settings -> Info** and use **Refresh Information** after replacing JGRF
or Geolith. Confirm:

- the JGRF executable and version are detected;
- the Geolith core and version are detected;
- the JG API version is shown;
- `Neo Geo CD Formats` includes `CUE, CHD` for a CHD build;
- `CHD Support` is `Available`;
- Vulkan is reported only when it is compiled into JGRF.

On Windows, inspect the actual DLL dependencies of the binaries being deployed:

```bash
objdump -p geolith.dll | grep "DLL Name"
objdump -p goliath-qt.exe | grep "DLL Name"
```

Do not reuse a dependency list from a different build.

---

## 8. Install and verify BIOS files

Set the BIOS directory in **Settings -> Path**, then run **Tools -> Verify
BIOS**.

The verifier recognizes:

```text
neogeo.zip
aes.zip
irrmaze.zip
neocd.zip
neocdz.zip
```

`neogeo.zip` and `aes.zip` cover the common MVS/AES cases;
`irrmaze.zip` is used for its special MVS hardware. For Neo Geo CD:

- Front Loader and Top Loader use their BIOS files from `neocd.zip`;
- their auxiliary `000-lo.lo` comes from `neocdz.zip`;
- CDZ and Universe BIOS modes use `neocdz.zip`.

The Geolith CD settings are stored in `config/jollygood/geolith.ini`:

```ini
[geolith]
cdsystem = 2
cd_dma_len_limit = 0
```

`cdsystem` values are:

```text
0 = Neo Geo CD Front Loader
1 = Neo Geo CD Top Loader
2 = Neo Geo CDZ
3 = Universe BIOS
```

`cd_dma_len_limit` is a 0/1 compatibility option and defaults to `0`.

Before each launch, Goliath prepares the BIOS location expected by unmodified
JGRF. On Windows this may create or refresh an NTFS junction beside the JGRF
executable. Keep the configured BIOS folder available and use the verifier
instead of manually duplicating archives into several locations.

---

## 9. Configure and rescan the library

### Media placement

- Put cartridge `.neo` files directly in the configured MVS/AES ROM directory.
  The current cartridge scan is not recursive; use the lowercase `.neo`
  extension.
- Put Neo Geo CD `.cue` and `.chd` files anywhere below the configured CD
  directory. CD discovery is recursive.
- Keep every CUE beside the track files referenced by its `FILE` statements,
  preserving the intended relative layout.
- Track files such as `.bin`, `.iso`, and `.wav` do not become separate games.

The configured cartridge directory must exist even for a CD-focused setup.

### First scan or rescan

1. Open **Settings -> Path**.
2. Select the MVS/AES ROM, Neo Geo CD, metadata, BIOS, icon, and snapshot
   directories.
3. Save the Path settings.
4. Choose **Tools -> Rescan ROMs**, or press **F5**.
5. Let the progress dialog finish.
6. Review its summary, then inspect both library views.

The worker rebuilds `database/games.json` outside the UI thread. The library is
reloaded automatically after a successful scan; no separate `goliath-db`
executable is required.

Rescan after changing media, scanner-relevant metadata, or any path used by the
scanner.

### Search the library

1. Type in **Search games...** to filter the current MVS/AES or Neo Geo CD
   view immediately.
2. Use the clear button inside the right side of the field to restore the
   complete list.
3. Press **Escape** to clear only while the search field has focus.
4. Press **Ctrl+F** from the main window to focus Search and select its current
   text, then type a replacement query directly.

---

## 10. Understand Neo Geo CD verification

CUE and CHD copies of the same title intentionally remain separate top-level
library entries because they are distinct launch media.

### Redump set verification (CUE/BIN)

For each CUE, Goliath:

1. parses every referenced `FILE` track;
2. resolves the reference inside the CUE directory without allowing parent
   traversal;
3. recovers case-only filename differences where the platform permits it;
4. compares every relevant BIN track's byte size and SHA-1 with the Redump
   catalog;
5. compares the CUE file's own byte size and SHA-1 with the Redump catalog.

A complete byte-perfect set receives:

```text
verification = redump-cue
badge = ✓ Redump set
```

If every BIN track matches but the CUE differs, Goliath preserves the trusted
disc identity and metadata while reporting the descriptor mismatch explicitly:

```text
verification = redump-tracks-only
badge = ⚠ CUE mismatch
```

Track hashes are cached in `database/hash_cache.json` using normalized path,
file size, and modification time. Stale cache rows are pruned only after a
complete scan with both the CD root and a valid Redump catalog available. If an
external CD drive is temporarily offline, the incomplete scan does not destroy
otherwise useful cache entries.

### MAME set verification (CHD)

For CHD v4/v5 images, Goliath reads the internal combined SHA-1 from the CHD
header and compares it with the `<disk sha1="...">` value in `neocd.xml`.

A match receives:

```text
verification = mame-chd
badge = ✓ MAME set
```

Do not compare `sha1sum game.chd` or a Redump Data SHA-1 with the MAME software
list value. The ordinary file hash, track hash, and CHD internal combined hash
are different values.

### Launch rules

Neo Geo CD is launched as:

```text
jollygood <configured arguments> -e neogeocd <media.cue|media.chd>
```

CUE is accepted directly. CHD is accepted only when the installed Geolith core
advertises `chd` for `neogeocd` through the JG API.

---

## 11. Configure Settings, input, and audio

The Settings dialog contains **Video**, **Audio**, **Misc**, **Core**,
**Input**, **Path**, and **Info** tabs. Each settings group has its own save
button. Closing the dialog does not imply that an unsaved tab was written.

### Input

1. Open **Settings -> Input**.
2. Select the input profile and, optionally, an SDL controller. Keyboard
   capture is always active.
3. Use **Refresh** if a newly connected SDL3 controller is missing.
4. Click a control, then press a keyboard key or move the controller input.
   Click the same waiting control again to cancel without changing its mapping.
5. Save the input configuration. Changing tabs, profiles, or controllers also
   cancels an unfinished capture safely.

Profiles cover Neo Geo Joysticks, Mahjong, 4-Player, V-Liner, Irritating Maze,
and their System controls. Controller labels use **Port 1 (`j0`)**, **Port 2
(`j1`)**, and so on: the visible port is one-based, while the value in
parentheses is JGRF's stored connection-order index. Keyboard mappings can
overlap JGRF frontend hotkeys. Press `Shift+Tab` inside JGRF to toggle its
hotkey processing when testing a conflicting binding.

### Per-game launch profiles

1. Select the exact parent, variant/hack, CUE, or CHD in the library.
2. Open **Tools -> Game Settings...**, or right-click and choose
   **Game settings...**.
3. On **General**, change only the system and input fields that should differ;
   leave the others on **Inherit global**.
4. On **Video**, override any renderer/window, CRTea, aspect, palette, or
   overscan value that should differ for this exact media. Use **Reset Video
   to Global** to clear only this tab without affecting General or mappings.
5. On **Input Mapping**, choose the controls to edit. The mapping-profile
   selector does not change **General -> Emulated Input**. Click a control and
   press a keyboard key or move the selected controller input.
6. A bullet marks an exact-media mapping. Right-click it and choose **Inherit
   global mapping** to clear only that control, or use **Reset shown mappings
   to Global** for the displayed mapping profile.
7. Choose **Save Profile**. A gear appears beside the row.
8. Use **Reset to Global** to remove every setting and input mapping for that
   media.
9. To inspect the generated INIs, right-click a row with a gear and choose
   **Open per-game config folder**. Goliath refreshes that exact media's
   runtime configuration before opening `config/p/<hash>/jollygood/`.

The profile can override the cartridge/CD BIOS, Universe BIOS hardware,
region, emulated input mode, the complete exposed JGRF/Geolith video surface,
MVS Free Play/Setting Mode, the SDL port used by inherited Player 1 joystick
bindings, and individual exact-media input mappings. Important constraints are:

- Free Play and Setting Mode require MVS hardware; AES and Neo Geo CD cannot
  enable them;
- four-player input requires direct MVS with JP or AS region;
- the controller choice retargets existing joystick bindings and does not
  replace the global button-mapping editor; exact-media mappings are applied
  afterward, so a captured `jN...` value retains its own port;
- unchanged controls inherit the global input INI. If that file does not yet
  exist, the runtime composition starts from Goliath's built-in keyboard
  defaults instead of leaving unrelated controls unmapped;
- V-Liner and Irritating Maze mapping panels are available, but their core
  input type remains auto-detected by Geolith;
- Vulkan is available as a per-game API only when the configured JGRF
  executable advertises it, and still requires the matching `shaders/*.spv`
  assets;
- per-game `--video` and `--shader` arguments override the effective global
  values only for that JGRF process;
- explicit fullscreen/windowed and initial-scale choices also receive final
  CLI precedence over conflicting global launch arguments;
- CRTea Mask Type/Strength, Scanline Strength, and Sharpness are used only by
  Custom mode; Curve, Corner, and Trinitron Curve apply to every CRTea preset;
- JGRF assigns `j0`, `j1`, ... by connection order, so reconnecting devices in
  another order can change which physical controller owns a saved port.

The profile database is `config/game_profiles.json`. It is independent from
scanner-generated `database/games.json`, so rescans do not remove profiles.
On every profiled launch, Goliath refreshes a generated configuration below
`config/p/<hash>/jollygood/`, points only that child JGRF process at it, and
continues to share the normal `data/` tree for BIOS, saves, states, and cheats.
Do not edit the generated `config/p/` copies by hand.

### Manage save states and persistent data

1. Close every running JGRF game.
2. Select the exact parent, variant/hack, CUE, or CHD in the library.
3. Choose **Tools -> Manage Save Data...**, or right-click and choose
   **Manage save data...**.
4. Confirm the displayed **Exact media**, **JGRF game name**, layout, state
   directory, and save directory.
5. Use **Create Backup** to write a manual exact-media snapshot.
6. Select a compatible archive and use **Restore Selected Backup** when
   required. Goliath creates a protective snapshot of current data first.
7. Use **Delete Selected File** only when intentionally resetting one state or
   persistent-data type. It also creates a protective snapshot first.
8. Use the three **Open ... Folder** actions to inspect the resolved upstream
   and Goliath-owned locations.

The manager accepts only these stock JGRF/Geolith files:

- state slots `0` and `1`: `.st0`, `.st1`;
- NVRAM: `.nv`;
- cartridge RAM: `.srm`;
- memory card: `.mcr`;
- Neo Geo CD backup RAM: `.brm`;
- DIP switches: `.dip`.

With a dynamic core (`-c geolith`), state and save data normally live under
`data/jollygood/state/geolith/` and `data/jollygood/save/geolith/`. A static
Windows JGRF keeps `state/` and `save/` beside its executable; a static Unix
build uses `data/geolith/state/` and `data/geolith/save/` with Goliath's
`XDG_DATA_HOME`. The dialog shows the resolved layout instead of asking the
user to choose paths manually.

Manual archives live below `data/goliath/save_backups/<hash>/`. Restore accepts
only a direct regular ZIP for the selected exact media and validates its
manifest, members, sizes, compression/encryption status, and CRC before any
live file is replaced. No launch-time automatic backups, arbitrary renaming,
or state slots above `1` are created.

JGRF derives live filenames from the final media basename, limited to 127
bytes before removing the extension. Consequently, media in different folders
with the same resulting basename share upstream live state/save files. Goliath
keeps their archive folders exact-media separated and displays both identities
so the distinction is explicit.

When a game launched by this running Goliath instance may still be active, the
dialog is read-only. A JGRF instance launched externally, or one that survived
a Goliath restart, cannot be detected automatically; close it manually before
creating, restoring, or deleting data.

### Use playtime tracking

1. Launch a parent, variant/hack, CUE, or CHD normally from the library.
2. Keep Goliath open while playing for a complete measurement.
3. Close JGRF normally, then select that exact row again if necessary.
4. Read **Playtime**, tracked session count, and **Last Played** in the details
   panel.

Normal launches are measured independently by system plus relative media path.
Sessions shorter than five seconds are ignored as probable launch failures,
and **Performance Benchmark** runs are never added. Statistics are replaced
atomically in `config/game_playtime.json` and survive rescans.

JGRF remains detached. Closing Goliath does not stop a running game; Goliath
stores the observed portion through launcher shutdown, but cannot measure the
remaining time after it exits. An unexpected launcher termination can also
lose the active, not-yet-persisted portion.

### Run a selected-game performance benchmark

1. Close other running games and heavy background applications.
2. Select the exact parent, variant/hack, CUE, or CHD to measure.
3. Right-click that row and choose **Benchmark...**, or use
   **Tools -> Benchmark Selected Game...**.
4. Select **Quick** (5,000 frames), **Standard** (10,000), **Extended**
   (30,000), or **Custom**.
5. Choose **Start Benchmark**. JGRF opens its normal render window, runs the
   requested frames while bypassing per-frame audio sample processing and
   using benchmark presentation behavior, then closes automatically.
6. Compare **Throughput** and **Average frame** only against another run using
   the same media, profile, video API, shader, hardware, and background load.
7. Use **Copy Result** when a text record is needed. The captured JGRF output
   is also appended to `jollygood.log`.

The benchmark uses the selected row's exact-media profile when one exists but
does not add a benchmark field to that profile or modify any global INI. Its
reported total begins when the managed JGRF child starts and ends when it
exits, so it includes core/media/window initialization and shutdown. It is
useful for repeatable regressions and renderer/shader comparisons, not for
audio quality, input latency, stutter, cross-game rankings, or a pure Geolith
CPU score. If Vulkan cannot use immediate presentation, FIFO may cap the run;
OpenGL is the safer baseline for uncapped comparisons.

### Export selected game audio to WAV

1. Select the exact cartridge parent, variant/hack, CUE, or CHD to record.
2. Choose **Tools -> Export Selected Audio WAV...**, or right-click and choose
   **Export audio WAV...**.
3. Accept the unique timestamped path below
   `data/goliath/audio_exports/`, or choose another existing writable
   directory and a new filename.
4. Review the destination and choose **Launch & Record**.
5. Use the game normally, then close JGRF normally so it can finalize the WAV
   header.

This is an explicit one-shot launch option. It is not stored in `goliath.ini`,
an exact-media profile, or generated JGRF/Geolith INIs. The selected profile
and session verbose mode still apply to the recording launch; Random and
Performance Benchmark do not inherit a pending export. The launch is ordinary
gameplay and therefore remains eligible for exact-media playtime tracking.

JGRF 2.0.1 writes 16-bit PCM output and refuses to overwrite any existing
target. Goliath enforces the same rule before launch and tests that the chosen
directory is writable. If a default timestamp already exists, the suggestion
receives ` - 2`, ` - 3`, and so on. If `[Paths] jollygood_args` already
contains `-o` or `--wave`, remove that advanced global writer while Goliath is
closed before using the explicit exporter.

Closing Goliath does not stop JGRF or the recording. A forced JGRF termination,
crash, power loss, or unwritable destination can leave an incomplete WAV, so
use the normal JGRF close action before opening the capture in another tool.

### Use verbose diagnostics and clear logs

1. Open **Tools -> Diagnostics & Logs...**.
2. Enable **verbose JGRF/core logging** when a normal Launch or Random launch
   needs extra upstream diagnostics.
3. Launch the affected game and inspect `jollygood.log` from the same dialog.
4. Disable the checkbox when the extra output is no longer needed.

The checkbox is intentionally session-only: it adds `--verbose` to subsequent
normal launch commands but does not write `goliath.ini`, JGRF/Geolith INIs, or
an exact-media profile. It is not applied to Performance Benchmark runs. If
`-v` or `--verbose` already exists in `[Paths] jollygood_args` inside
`goliath.ini`, the checkbox is shown as globally configured and Goliath does
not append a duplicate. Such an explicit global argument continues to affect
benchmarks. Edit that advanced value only while Goliath is closed.

The dialog shows the current location, size, and modification time for both
logs and provides separate Open and Clear actions. Clearing is permanent and
requires confirmation:

- `goliath-qt-debug.log` is truncated through its synchronized live logger;
  later frontend events continue writing to the same file;
- `jollygood.log` is cleared only when it is a direct regular file and no JGRF
  process tracked by the current Goliath session may still be active;
- a missing log is simply reported as not created and no placeholder is made;
- directories, symbolic links, and other non-regular targets are refused.

Goliath cannot safely detect a JGRF process started externally or one that
survived a Goliath restart. Close such processes manually before clearing
`jollygood.log`; otherwise their still-open output handle may continue writing
to the file.

### Audio

The Audio tab enumerates playback devices asynchronously so opening Settings
does not wait for a slow Windows audio endpoint. JGRF still uses SDL's system
default output; the detected list is informational.

If no playback device is available at launch, Goliath warns before starting
JGRF and offers **Cancel** or **Launch Anyway**. Connect an output device and
use **Settings -> Audio -> Refresh** before retrying.

On Windows, Goliath stores the selected volume and retries until it can apply
that value to the new JGRF audio session. Per-process volume application is
currently Windows-only; JGRF remains unmodified.

---

## 12. Deploy locally and on a clean Windows PC

### Local development copy

From the MSYS2 project directory:

```bash
cp build/goliath-qt.exe ../Goliath/goliath-qt.exe
```

PowerShell equivalent:

```powershell
Copy-Item `
  .\build\goliath-qt.exe `
  ..\Goliath\goliath-qt.exe `
  -Force
```

Copying only the frontend is sufficient when the existing local runtime
directory already contains the correct Qt, SDL3, JGRF, Geolith, shader, and
user-supplied BIOS files.

### Deployment without MSYS2

Run the Qt deployment tool that matches the Qt installation used for the
build:

```bash
windeployqt6.exe --release goliath-qt.exe
```

Depending on the Qt package, the command may instead be named
`windeployqt.exe`.

A clean deployment normally needs:

- `goliath-qt.exe`;
- Qt6 DLLs and plugins, including `platforms/qwindows.dll`;
- `SDL3.dll`;
- the configured JGRF executable and its runtime files;
- `cores/geolith/geolith.dll` and its actual runtime dependencies;
- JGRF `shaders/*.spv` files when Vulkan is enabled;
- `LICENSE`, `THIRD-PARTY-NOTICES.md`, and the complete `licenses/` directory.

ROMs, disc images, BIOS/firmware files, encryption keys, game artwork, and
external metadata catalogs are not release payload files. Users add their own
content after obtaining the software package.

The Goliath application target statically links the MinGW libgcc, libstdc++,
and winpthread runtimes. Qt and SDL3 remain dynamic dependencies.

Do not copy a CMake `build/` directory to another machine or source location.
Its cache contains absolute paths and is not a runtime package.

Validate the package on a clean PC or VM without MSYS2 before release.
Follow `docs/RELEASE_COMPLIANCE.md` before publishing any binary archive.

---

## 13. Logs and troubleshooting

Start with these files beside the Goliath executable:

```text
goliath-qt-debug.log          frontend paths, launch decisions, and errors
jollygood.log                merged JGRF/Geolith stdout and stderr, appended
```

Use **Tools -> Diagnostics & Logs...** to open either file, enable transient
verbose JGRF/core output, or clear each log independently. Clearing
`jollygood.log` is blocked while a process tracked by this Goliath session is
still active.

### Goliath does not start

- Run `objdump -p goliath-qt.exe | grep "DLL Name"` on the shipped executable.
- Confirm the Qt DLLs, `SDL3.dll`, and `platforms/qwindows.dll` are present.
- Run `windeployqt` from the same Qt toolchain that built the executable.
- Test outside the MSYS2 shell so missing deployment dependencies are visible.

### CMake refers to an old source location

Do not reuse that cache. Configure a new directory such as
`build-validation/`, as shown in section 3.

### JGRF or Geolith is unavailable in Settings -> Info

- Confirm `[Paths] jollygood` resolves to the real executable.
- On Windows, the `.exe` suffix may be omitted from the configured value.
- Confirm the Geolith core is exactly below
  `cores/geolith/geolith.dll` relative to that executable.
- Inspect the core's dependent DLLs and retry **Refresh Information**.

### CHD is identified but launch is blocked

- Open **Settings -> Info** and inspect `Neo Geo CD Formats` and `CHD Support`.
- Rebuild Geolith with both `ENABLE_CHDR=1` and `-DHAVE_CHDR`.
- Replace the deployed core and refresh the Info tab.
- Read `goliath-qt-debug.log` for capability-probe details.

### A CUE does not receive the Redump badge

- Confirm the metadata folder contains a valid Redump Neo Geo CD `.dat`.
- Check that every CUE `FILE` reference resolves to the correct track.
- Confirm track sizes and SHA-1 values match the same Redump catalog entry.
- Rescan and review the progress output.
- Delete `hash_cache.json` only when deliberately forcing every CUE track to be
  rehashed; normal cache invalidation already follows size and modification
  time.

### A CHD does not receive the MAME badge

- Confirm `metadata/neocd.xml` is present and current for the image.
- Compare the CHD internal combined SHA-1, not the whole-file SHA-1.
- Confirm the CHD is version 4 or 5 and rescan.

### A game does not appear

- Confirm the configured cartridge directory exists.
- Put `.neo` files directly in that directory and use the lowercase extension.
- Confirm CUE/CHD files are below the configured CD directory.
- Review the rescan summary and `database/games.json`.
- Confirm `neogeo.xml`, `neocd.xml`, and supplemental metadata paths are
  correct.

### A game does not launch

- Review `goliath-qt-debug.log`, then `jollygood.log`.
- Verify the JGRF path, selected media path, BIOS report, and deployed core.
- For MVS/AES, confirm the selected `main_rom` exists under the ROM root.
- For CHD, also confirm the installed capability result in Settings -> Info.

### A per-game profile does not apply

- Confirm the selected row has the gear marker and reopen **Game Settings**.
- Check `goliath-qt-debug.log` for `per-game launch profile active` and the
  effective `XDG_CONFIG_HOME` path.
- Keep the configured `config` root short enough for JGRF 2.0.1's fixed
  configuration-path buffer. Goliath blocks the launch instead of allowing a
  truncated path.
- Unchanged controls inherit **Settings -> Input**. If no global input INI has
  been saved yet, Goliath uses its built-in keyboard defaults for the isolated
  runtime copy.
- A custom joystick mapping keeps its captured `jN` port and is applied after
  the general Player 1 controller-port retargeting.
- Remember that controller ports follow the devices' current SDL connection
  order.

### A benchmark fails or appears capped

- Read the result text, `goliath-qt-debug.log`, and the newly appended portion
  of `jollygood.log`.
- Confirm normal launch preflight succeeds for the same selected media and
  exact-media profile.
- Use OpenGL for the uncapped baseline. Vulkan falls back to FIFO when the
  driver does not expose immediate presentation and can therefore remain
  synchronized to the display.
- Close another running JGRF instance and heavy background applications before
  comparing results.
- Do not compare different games, media revisions, profiles, shaders, or
  renderers as though they were the same workload.

### Playtime does not update as expected

- Use normal **Launch**; Benchmark is excluded deliberately.
- Keep the game open for at least five seconds and allow the one-second
  observer interval before checking the selected row.
- Keep Goliath open for a complete session. If it closes first, only the
  already observed portion is stored.
- Check `goliath-qt-debug.log` for process-observer or atomic-save errors.
- If `config/game_playtime.json` is malformed, Goliath preserves it instead of
  overwriting it automatically; repair or move that file after making a
  backup.

### Save data is missing, shared, or cannot be changed

- Reopen **Manage Save Data...** and verify the displayed JGRF game name and
  dynamic/static layout. The manager follows upstream naming, not the library
  display title.
- Two media paths with the same final basename intentionally resolve to the
  same stock-JGRF live filenames. Their Goliath backup folders remain separate.
- If the dialog is read-only, close every JGRF game launched by the current
  Goliath session and press **Refresh**.
- Also close any JGRF process launched outside Goliath or left running across a
  Goliath restart; it cannot be identified safely by this dialog.
- An archive marked **Invalid / incompatible** is never restorable. Hover its
  status for the validation reason; do not edit its manifest to bypass the
  exact-media check.
- A failed restore reports whether rollback also failed. Preserve the
  protective archive and inspect both live folders before retrying.

### Vulkan fails at startup

Confirm the shader assets generated by the same JGRF build are present under
`shaders/`. Recheck JGRF's upstream runtime layout.

### OpenGL ES fails with `glGenBuffers() not found`

This is a known issue with the currently tested JGRF 2.0.1 OpenGL ES path.
Select another renderer while that separate upstream/context issue remains
unresolved.

### No audio output is detected

Connect speakers, headphones, or another playback device, then use
**Settings -> Audio -> Refresh**. JGRF uses the system-default SDL output, so
also verify the operating system's default playback device.

### A Windows path exceeds `MAX_PATH`

Do not manually add a `\\?\` prefix to Settings, `games.json`, or
`hash_cache.json`. Goliath keeps stored paths normal and converts only the final
external CD media argument to extended-length form when required. Relative CUE
track references remain supported.

---

## 14. Change-specific validation

### UI or Settings change

```bash
cmake --build build-tests -j$(nproc)
./build-tests/goliath-qt-tests
```

Then deploy the rebuilt application, open every affected tab, save and reopen
the changed settings, and launch at least one representative game.

### Scanner, metadata, or GameModel change

```bash
cmake --build build-tests -j$(nproc)
./build-tests/goliath-qt-tests
```

Then run a real rescan and verify:

- the scan summary;
- `database/games.json`;
- both MVS/AES and Neo Geo CD library views;
- parent/variant grouping;
- CUE and CHD verification badges;
- selection, sorting, search, and Random behavior.

### Launch, BIOS, JGRF, or Geolith change

After automated tests, manually launch:

1. one MVS/AES game;
2. one Neo Geo CD CUE;
3. one Neo Geo CD CHD;
4. Settings, including Info and Audio;
5. a visible game selected through Random.

For a per-game profile change, also test one MVS override, one AES override,
one CUE or CHD override, one alternate Video API, shader `0`, fullscreen and
scale, one CRTea preset plus Custom tuning, aspect/palette/overscan, video-only
reset, whole-profile reset, and a profiled Random launch. Confirm the selected
API and shader in a Benchmark result.
Map at least one keyboard key and one controller input for an exact media item;
confirm a right-click return to inheritance and test the V-Liner/Irritating
Maze mapping panels without changing the core input type.
Open the generated config folder for a parent and a variant or for a CUE and a
CHD, and confirm that each selection resolves to its own directory. Confirm
that the global files under `config/jollygood/` remain unchanged.

For a benchmark change, run Standard once for an MVS/AES row and once for a
CUE or CHD. Confirm automatic JGRF exit, a successful completion marker,
non-zero time/FPS/frame-time results, Copy Result, and Cancel Benchmark. Repeat
the same media/profile twice to check that results are reasonably stable, then
confirm a normal Launch still starts detached and is unaffected.

For a playtime change, use one normal cartridge launch and one CUE or CHD
launch for more than five seconds. Confirm exact-media totals/session counts,
local Last Played display, persistence after restarting Goliath, exclusion of
a Benchmark run, and the partial-save behavior when closing Goliath before a
detached game.

For a save-data-manager change, create representative `.st0`/`.st1` and
persistent data through normal gameplay, then confirm inventory, manual backup,
restore after changing a file, stale-file removal, protective backup creation,
one live-file deletion, and backup deletion. Repeat for one cartridge and one
CUE or CHD. While a Goliath-launched game is running, confirm every mutating
action is disabled but folders and inventory remain readable. Place a corrupt
or foreign ZIP directly in the exact-media backup folder and confirm it is
shown as invalid and cannot be restored.

For a diagnostics/logging change, enable the session toggle and launch one
game directly plus one through Random. Confirm exactly one `--verbose` appears
before the final media argument in `goliath-qt-debug.log` and that JGRF emits
verbose output. Disable it and confirm the option disappears. Run a Benchmark
and confirm the session toggle is not added. Clear each log, verify zero size,
then cause a new frontend event and normal launch to confirm both files resume
logging. While a tracked game is active, confirm only JGRF-log clearing is
disabled; also verify an explicit global `-v` is recognized without
duplication.

For an audio-export change, record one MVS/AES title, one long-path CUE, and
one CHD. Confirm exactly one `--wave <new path>` pair appears before the final
media argument, each WAV grows while playing and opens after a normal JGRF
exit, and CUE/CHD profiles remain exact-media isolated. Repeat with verbose
enabled, close Goliath before JGRF once, reject an existing target, and confirm
Benchmark and a later ordinary launch receive no `--wave` option.

Review both logs after any failure.

---

## 15. Final validation checklist

Use a new, previously unused directory for an independent clean configure and
build. The `build-validation/` name below is only an example:

```bash
cmake -S . -B build-validation -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_TESTS=ON
cmake --build build-validation -j$(nproc)
./build-validation/goliath-qt-tests
```

Before calling a release checkpoint complete, confirm:

- the complete suite reports no failures;
- `goliath-qt` and `goliath-qt-tests` were both produced by the clean build;
- MVS/AES, CUE, CHD, Settings, and Random smoke tests pass;
- selected-game benchmark completion, result copying, and cancellation pass
  without changing normal detached launches or saved profiles;
- normal exact-media playtime persists, Benchmark remains excluded, and no
  JGRF process is terminated by the tracker;
- save-data backup/restore/delete creates the documented protective archives,
  rejects an invalid or foreign ZIP, and stays read-only during a tracked game;
- session verbose logging affects normal/Random launches but not Benchmark,
  and both log clear actions resume writing after truncation;
- one-shot WAV export preserves exact-media profiles, refuses overwrite,
  survives Goliath close, finalizes on normal JGRF exit, and does not leak into
  Random, Benchmark, or a later normal launch;
- Settings -> Info reports the installed JGRF/Geolith capabilities correctly;
- the deploy package works without MSYS2;
- no build directory, patch, backup, generated database, or external metadata
  bundle is accidentally included in the source repository;
- the final repository inventory contains only intentional source,
  documentation, assets, tests, and vendored dependencies.
