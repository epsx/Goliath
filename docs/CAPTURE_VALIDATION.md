# Capture Validation Matrix

This is the release acceptance checklist for Goliath's game-window PNG
capture, experimental GIF recording, and Snaps/GIFs gallery. Run it against
the exact candidate binary that will be published. It intentionally uses a
small pairwise matrix instead of every possible combination.

Goliath captures only games launched and tracked by the current frontend
session. JGRF, JG, Geolith, and Lithogen remain separate components.

## Automated gates

All gates must pass before manual testing:

```bash
cmake -S . -B build-tests -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_TESTS=ON
cmake --build build-tests -j2
ctest --test-dir build-tests --output-on-failure
```

The regular test target includes these focused regressions:

```bash
./build-tests/goliath-qt-tests "[gif]" --reporter compact
./build-tests/goliath-qt-tests "[hotkeys]" --reporter compact
./build-tests/goliath-qt-tests "[library-view]" --reporter compact
./build-tests/goliath-qt-tests "[theme]" --reporter compact
```

On Windows, append `.exe` to the test executable. A release build must produce
zero compiler warnings and zero test failures.

The GitHub Linux job installs X11 and PipeWire development files and configures
with both `GOLIATH_REQUIRE_X11_CAPTURE=ON` and
`GOLIATH_REQUIRE_WAYLAND_CAPTURE=ON`. This prevents CI from silently compiling
a Linux artifact without either capture backend.

## Common checks

For every successful GIF row below:

- the game remains responsive during recording;
- the saved file opens, loops, contains motion, and has the selected duration;
- the image is not black, frozen, stretched, letterboxed, or offset;
- the file is stored under
  `recordings/<exact-media-id>/<YYYY-MM-DD>/<HH-mm-ss>.gif`;
- existing recordings remain untouched;
- the status bar returns to normal library information after its temporary
  saved/failed message;
- MVS/AES parent/variant IDs and Neo Geo CD sanitized filenames remain
  separate.

On Windows and X11, focus another running application while a capture is
requested. Goliath must refuse or cancel safely rather than save the wrong
game. On Wayland, the portal deliberately records the window chosen by the
user; use the dedicated cancellation row below instead.

## Windows acceptance

Use the MSYS2 UCRT64 release candidate and the same JGRF/Geolith runtime that
will accompany the release.

| ID | Media | Renderer/mode | Start | Duration | Expected result | Result |
| --- | --- | --- | --- | --- | --- | --- |
| W1 | MVS/AES | OpenGL, windowed | GIF hotkey | 5 s | Client area only; moving GIF | ☐ |
| W2 | Neo Geo CD | OpenGL, windowed | Tools countdown | 7 s | Correct CD recording folder | ☐ |
| W3 | MVS/AES | Vulkan, windowed | GIF hotkey | 10 s | Visible, moving, complete GIF | ☐ |
| W4 | Neo Geo CD | Vulkan, fullscreen | GIF hotkey | 7 s | Desktop Duplication; no bars or frozen frames | ☐ |
| W5 | MVS/AES | OpenGL, fullscreen | Tools countdown | 7 s | Focused game only; moving GIF | ☐ |
| W6 | MVS/AES | OpenGL, windowed | PNG Tools action | still | Preview excludes title/status bars; save succeeds | ☐ |
| W7 | Neo Geo CD | Vulkan, windowed | PNG hotkey or Tools | still | Non-black preview and saved PNG | ☐ |

Then open the selected game's gallery:

- ☐ **Snaps** and **GIFs (n)** switch without shrinking the media viewport;
- ☐ previous/next and the `n/total` counter stay synchronized;
- ☐ play/pause changes only GIF playback;
- ☐ wide and 4:3 GIFs fill the 500 × 370 viewport by centered crop without
  stretching;
- ☐ the context menu shows **Open GIF Folder** only when that exact game has
  recordings and opens the correct directory;
- ☐ hover text does not expose filesystem paths.

## Linux X11 acceptance

Configure with `GOLIATH_REQUIRE_X11_CAPTURE=ON` and confirm Qt uses `xcb`.
PNG capture is intentionally unavailable.

| ID | Media | Renderer/mode | Start | Duration | Expected result | Result |
| --- | --- | --- | --- | --- | --- | --- |
| X1 | MVS/AES | OpenGL, windowed | GIF hotkey | 5 s | Immediate focused-window capture | ☐ |
| X2 | Neo Geo CD | OpenGL, windowed | Tools countdown | 7 s | Starts after return-to-game delay | ☐ |
| X3 | MVS/AES | Vulkan, fullscreen | GIF hotkey | 10 s | Moving GIF from focused JGRF window | ☐ |
| X4 | Neo Geo CD | Vulkan or OpenGL, fullscreen | GIF hotkey | 7 s | Correct CD recording folder | ☐ |

Expected X11 limitation: Qt reads visible pixels. A window placed over the
game, including the command companion, can appear in the GIF. Keep the game
stationary, focused, and unobscured. This is not a wrong-game routing failure.

Repeat the gallery checks from the Windows section for one MVS/AES title and
one Neo Geo CD title.

## Linux Wayland acceptance

Configure with `GOLIATH_REQUIRE_WAYLAND_CAPTURE=ON`, confirm Qt uses
`wayland`, and verify a ScreenCast portal plus PipeWire are running. PNG
capture is intentionally unavailable.

| ID | Media | Renderer/mode | Start | Duration | Expected result | Result |
| --- | --- | --- | --- | --- | --- | --- |
| L1 | MVS/AES | OpenGL, windowed | Tools | 5 s | Portal picker; selected window fills the GIF | ☐ |
| L2 | Neo Geo CD | OpenGL, windowed | approved GIF shortcut | 7 s | Portal picker; correct CD folder | ☐ |
| L3 | MVS/AES | Vulkan, fullscreen | approved GIF shortcut | 10 s | Moving PipeWire capture; no corner offset | ☐ |
| L4 | Neo Geo CD | Vulkan or OpenGL, fullscreen | Tools | 7 s | Moving GIF; gallery playback succeeds | ☐ |
| L5 | either | any | cancel portal picker | — | Informative cancellation; no crash or partial GIF | ☐ |

The portal picker appearing for every recording is expected. Desktop-reserved
shortcuts such as `Ctrl+Alt+F3` or `Ctrl+Alt+F12` may be unavailable; choose a
free key and approve it in the GlobalShortcuts dialog. If a binding already
exists, use **Settings > Hotkeys > Open Desktop Shortcut Settings...**, change it
in the desktop UI, and confirm that the displayed active desktop shortcut
and the activation log both update. Window stacking and restored positions for
the separate `command.dat` companion remain compositor controlled and are not
capture failures.

## Release sign-off

Record the tested commit, binary SHA-256, JGRF/Geolith versions, desktop
session, GPU/driver, and any failed row. Do not mark capture release-ready if a
GIF is black, static, routed to another game, saved under the wrong media ID,
or missing from the gallery.

The GIF feature remains experimental even after every row passes. Video
recording with audio is future work and is outside this matrix.
