# Windows OpenGL ES through WGL — Runtime Fix01

Date: 2026-09-06

Status: validated and deployed in the controlled Windows runtime

Scope: external runtime compatibility for JGRF; no Goliath or Geolith source
change

## Purpose and ownership

This document records two independent defects encountered when JGRF 2.0.1 ran
Geolith 0.4.2 through OpenGL ES on Windows 11. Both defects had to be corrected
before the renderer produced an image.

The patches are local compatibility fixes based on pinned upstream sources.
They are not represented as changes accepted by the upstream projects.

| Component | Role | Fix01 state |
|---|---|---|
| Goliath Qt/C++ | Selects media and launches JGRF | Unchanged |
| JGRF 2.0.1, `jollygood.exe` | Creates the graphics context and renders the core video buffer | BGRA allocation patched |
| JG API 2.0.0 | Contract between JGRF and Geolith | Unchanged |
| Geolith 0.4.2 | Neo Geo emulation core | Unchanged |
| libepoxy 1.5.10 | Resolves OpenGL entry points used by JGRF | WGL ES dispatch patched |
| SDL3 3.4.12 | Creates the window/context and supplies input/audio services | Unchanged |
| Vulkan loader and shaders | Alternate JGRF renderer | Preserved |

The files share one Windows deployment directory so the loader can resolve the
DLL dependencies. That location does not make the fixes part of Goliath's
application source.

## Tested environment

| Item | Value |
|---|---|
| Operating system | Windows 11 |
| GPU | NVIDIA GeForce RTX 5070 Ti |
| Driver | NVIDIA 616.56 |
| OpenGL ES context | OpenGL ES 3.2 through WGL |
| SDL3 runtime | 3.4.12 (`SDL_version=3004012`) |
| JGRF | 2.0.1 |
| JG API | 2.0.0 |
| Geolith | 0.4.2 |
| libepoxy source base | 1.5.10 plus MSYS2 `1.5.10-7` patches |

## Failure sequence

### 1. Missing OpenGL ES function dispatch

The original launch loaded the game and printed its ROM metadata, then aborted
with:

```text
glGenBuffers() not found:
```

Windows Event Viewer recorded `jollygood.exe`, `ucrtbase.dll`, exception code
`0xc0000409`. The module named by that event was not sufficient to identify the
cause; the controlled probes isolated the abort to libepoxy's failed symbol
lookup.

The same WGL OpenGL ES context resolved and executed `glGenBuffers` through
SDL3. The unpatched installed and reconstructed libepoxy DLLs failed only when
the call passed through libepoxy. Desktop Core and Compatibility contexts
worked.

### 2. Black frame after dispatch was corrected

After the corrected libepoxy DLL was installed, JGRF no longer crashed. Geolith
ran and audio was present, but the game window remained black for cartridge,
CUE, and CHD media.

The JGRF XRGB8888 path supplied `GL_BGRA` as the external format while using
`GL_RGBA` as the internal format. In the tested OpenGL ES context,
`glTexImage2D` rejected this combination with `GL_INVALID_OPERATION` (`0x0502`).
The resulting framebuffer was incomplete (`0x8cd6`), draw/readback operations
returned `GL_INVALID_FRAMEBUFFER_OPERATION` (`0x0506`), and both passes
produced black pixels.

The context reported `GL_EXT_texture_format_BGRA8888`. Using `GL_BGRA` for both
the internal and external format made the same two-pass path complete and
produce non-black pixels without a GL error.

## Fix A — libepoxy WGL dispatch

Patch:

```text
Patches/GOLIATH_LIBEPOXY_WGL_ES_FIX01.patch
```

The patch changes only `src/dispatch_common.c` in libepoxy:

1. When a current WGL context exists, GLES1/2/3 resolution first calls
   `wglGetProcAddress`.
2. It rejects null and the known invalid WGL sentinel values `1`, `2`, `3`,
   and `-1`.
3. It falls back to the existing `opengl32.dll` lookup for older exported GL
   entry points.
4. Existing non-WGL and desktop OpenGL resolution paths remain unchanged.

This follows the context actually created by SDL3. It does not introduce an
arbitrary GLES DLL or change the renderer selected in JGRF configuration.

## Fix B — JGRF OpenGL ES BGRA allocation

Patch:

```text
Patches/GOLIATH_JGRF_OPENGL_ES_BGRA_FIX01.patch
```

The patch changes only `src/video_gl.c` in JGRF. A small helper selects the
texture internal format:

- OpenGL ES plus an external `GL_BGRA` format uses internal `GL_BGRA`;
- desktop OpenGL retains the existing `pixfmt.format_internal` value;
- RGBA and OSD paths remain unchanged.

The helper is used by the five source/output texture allocations covering
initial setup, resize/refresh, and the compatibility setup path. It does not
change JG video buffers, Geolith, shaders, input, audio, or Vulkan rendering.

## Source provenance

| Source | Pinned identity |
|---|---|
| libepoxy | tag `1.5.10`, commit `c84bc9459357a40e46e2fec0408d04fbdde2c973` |
| MSYS2 libepoxy recipe | `1.5.10-7`, with its four recipe patches preserved in the build package |
| JGRF | tag `2.0.1`, commit `c080341ab2bc72c0574ffb5dc36203096b5465cb` |
| JG API | tag `2.0.0`, commit `43af3ed5bf6e4c97da4f564f3ef0626796b352ef` |
| Geolith | runtime reports version `0.4.2`; source unchanged |

The libepoxy source archive is an assembled MSYS2 recipe base, not an
unmodified upstream release archive. The packages preserve the upstream
license, recipe, applied MSYS2 patches, local patch, and build instructions.

## Patch and binary identities

| Artifact | SHA-256 |
|---|---|
| Original runtime `libepoxy-0.dll` | `dc4055b53d0d6507740e9f3be18c9f2b1318faa531b9b2913eef3c5332fffedc` |
| Reconstructed libepoxy baseline | `dc88057cc412fa05a304a5f7c55d17795ede37c980cfa19549ef87a1ae70da2d` |
| Deployed Fix01 `libepoxy-0.dll` | `50ff6e67ccc5d76bd3bc157eb26a7fbe1a15743574b3f0fa1de50eb997c79da3` |
| libepoxy Fix01 patch | `57b764d9e423886f927fb6f777a46d73bb7ddec42baffc2b115c2bf5f1502d65` |
| Original runtime `jollygood.exe` | `f411acfdd7822535a21919247e67153b6b3e36516bfe47596a1b996eeea555ac` |
| Reconstructed Vulkan JGRF baseline | `47379033128aa9795bc9874dfed3b1af15f29d533239f6d83d46499595a4217c` |
| Deployed Vulkan JGRF Fix01 candidate | `d2c9330fe885fcf3cc35073ad84e4f42fc4f556282abce924a5cf60819481b7f` |
| JGRF BGRA Fix01 patch | `c424ee3b5a0170f3d992824e7e91430ce3f9ce01f91879eee3f0b70db613268a` |
| Unchanged runtime `SDL3.dll` | `30094fadba1264531c27b0eb848aae5c3a4addc691a296f14deeaef725479dfd` |

The reconstructed libepoxy baseline can differ byte-for-byte from the
previously bundled DLL while preserving the failure and export behavior. The
source provenance and paired baseline/candidate build are the authoritative
comparison for the local libepoxy patch.

Build and deployment packages used for this investigation:

| Package | SHA-256 |
|---|---|
| `GOLIATH_LIBEPOXY_WGL_ES_FIX01_PACKAGE.zip` | `505e50ef22e7748ed0975424c0326d7792747adce1aaf5672490cc3953010909` |
| `GOLIATH_LIBEPOXY_WGL_ES_FIX01_RENDER_DIAGNOSTIC_V2_PACKAGE.zip` | `9e7678d4de5a2edfecd6f4644f7465d92214307763fa3a7bf3aab0f12e11032c` |
| `GOLIATH_JGRF_OPENGL_ES_BGRA_FIX01_PACKAGE.zip` | `62f3d08a42ee315bdf03caabeb715eaae47360ebaa0539af02ecd77bd95867be` |
| `GOLIATH_JGRF_OPENGL_ES_BGRA_FIX01_VULKAN_BUILD_PACKAGE.zip` | `98372a89f0527a32c17bbfa37ffb8ffbe1a581404eec09211d45bf71cb9aaf38` |
| `GOLIATH_JGRF_OPENGL_ES_BGRA_FIX01_CONTROLLED_DEPLOY_PACKAGE.zip` | `5d4a5f5bb53e4f4848b8be6f7275dfa7dfb144ed8cc0ebff32cbee46f5810417` |

## Validation evidence

### Dispatch isolation

The diagnostic compared SDL and libepoxy resolution in Core, OpenGL ES, and
Compatibility contexts without launching Goliath, JGRF, Geolith, a ROM, or a
BIOS.

| Context | SDL resolution | Unpatched libepoxy | Fix01 libepoxy |
|---|---:|---:|---:|
| Core | PASS | PASS | PASS |
| OpenGL ES | PASS | abort/failure | PASS |
| Compatibility | PASS | PASS | PASS |

The isolated Fix01 build reproduced the ES failure in both the installed DLL
and reconstructed baseline, then passed all six SDL/libepoxy candidate probes:

```text
unexpected_probe_results=0
fix01=PROBES_PASSED runtime_deployed=no games_tested=no
```

### Two-pass JGRF rendering probe

The probe reproduced JGRF's source texture, framebuffer texture, first draw,
and final draw without loading game data:

| Case | Failures | Result |
|---|---:|---:|
| Current JGRF `GL_RGBA` / `GL_BGRA` | 7 | FAIL, black output |
| Proposed JGRF `GL_BGRA` / `GL_BGRA` | 0 | PASS, non-black output |
| RGBA control | 0 | PASS, non-black output |

Final marker:

```text
jgrf_bgra_fix_candidate=VALIDATED_BY_RENDER_PROBE
```

### JGRF reproducible builds

The first build used `ENABLE_VULKAN=0` only to prove the source change compiled;
that executable was never deployed because it would have removed a feature
present in the installed JGRF.

The final baseline and candidate were rebuilt with `ENABLE_VULKAN=1`. Acceptance
required:

- the same direct DLL import set as each other and the installed executable;
- an explicit `vulkan-1.dll` import;
- all eight SPIR-V shaders with baseline/candidate identity;
- successful help-mode smoke tests;
- unchanged runtime files throughout the isolated build.

```text
candidate_import_set_matches_baseline=OK
candidate_import_set_matches_installed=OK
candidate_vulkan_import=OK
vulkan_shader_identity_preserved=OK
jgrf_bgra_fix01_vulkan=BUILD_PASSED runtime_deployed=no games_tested=no
```

### Controlled deployment and game smoke tests

The deployment script preserved the exact original `jollygood.exe`, verified
the complete Vulkan build evidence, staged the candidate, and proved that the
runtime inventory and every non-JGRF runtime file remained unchanged.

```text
checkpoint exact original backup and candidate preservation exit=0
checkpoint exact candidate deployment exit=0
checkpoint all non-JGRF runtime files and inventory unchanged exit=0
original_jollygood_preserved=yes
libepoxy_modified=no SDL3_modified=no
jgrf_bgra_fix01_vulkan=DEPLOYED controlled_game_test_authorized=yes
```

Deployment audit:

```text
Diagnostics/
  JGRF-OpenGL-ES-BGRA-Fix01-Deployment-20260906_202943-2047/
```

The user then confirmed functional image and input through OpenGL ES for:

- Neo Geo MVS/AES media;
- Neo Geo CD CUE media;
- Neo Geo CD CHD media.

The previous audio-only execution demonstrated that emulation and audio had
continued while the JGRF video path was black. Fix01 changes no audio code.

## Evidence log identities

| Evidence | SHA-256 |
|---|---|
| OpenGL ES dispatch diagnostic V2 | `79191afa1f23ce5a21524ccc91c41a79b1feeea739e8a3483240d8c19ceb3354` |
| libepoxy paired build and probes | `e8df8032b30d3aba0b142deaf6c9c44f0f65afeed8dd56ed534cfc6809dc1736` |
| JGRF two-pass render diagnostic V2 | `ce01700efce81ca4974e693ae735d6030047497d9165adcd81806bd5edbd7757` |
| JGRF Vulkan paired build | `70eec9a4ccecf4b26f0401f814440a658bcb260876b94da425cb425ff5e76cc9` |

## Recovery

The JGRF deployment package contains:

```text
GOLIATH_JGRF_OPENGL_ES_BGRA_FIX01_RECOVERY.sh
```

It accepts only the exact deployed candidate or an already restored original,
locates a hash-verified deployment backup, preserves the candidate, and
restores only `jollygood.exe`. It intentionally does not change
`libepoxy-0.dll`, SDL3, configuration, saves, ROMs, BIOS files, or Goliath.

The libepoxy build/deployment evidence must remain available separately because
the two fixes affect different upstream components.

## Distribution and maintenance rules

1. Bundle the patched `jollygood.exe` and `libepoxy-0.dll` as a matched Windows
   OpenGL ES runtime pair.
2. Preserve the patch files, source provenance, license files, build scripts,
   binary hashes, and recovery evidence with the release archive.
3. Keep Vulkan shader assets when distributing the Vulkan-enabled JGRF build.
4. Do not describe these local patches as accepted upstream changes unless an
   upstream project actually accepts an equivalent correction.
5. Revalidate both isolated probes and real MVS/AES, CUE, and CHD launches when
   updating SDL3, libepoxy, JGRF, the GPU driver, or the compiler toolchain.
6. Meson and Python are libepoxy build tools only. Neither Fix01 binary adds a
   Python runtime dependency to Goliath or JGRF.

## Remaining limits

- Hardware validation currently covers one NVIDIA/Windows/WGL environment.
- EGL/ANGLE, native GLES DLLs, GLES1 hardware, AMD, Intel, and multiple-context
  switching require separate validation.
- The BGRA correction relies on `GL_EXT_texture_format_BGRA8888`, which was
  explicitly present in the tested context.
- Patch redistribution and upstream attribution must continue to follow the
  respective project licenses; this technical record is not a license audit.

## References

- [libepoxy 1.5.10 `dispatch_common.c`](https://github.com/anholt/libepoxy/blob/1.5.10/src/dispatch_common.c)
- [MSYS2 libepoxy recipe](https://github.com/msys2/MINGW-packages/tree/master/mingw-w64-libepoxy)
- [JGRF project](https://gitlab.com/jgemu/jgrf)
- [JG project](https://gitlab.com/jgemu/jg)
- [OpenGL EXT texture format BGRA8888](https://registry.khronos.org/OpenGL/extensions/EXT/EXT_texture_format_BGRA8888.txt)
- [Microsoft `wglGetProcAddress`](https://learn.microsoft.com/windows/win32/api/wingdi/nf-wingdi-wglgetprocaddress)
