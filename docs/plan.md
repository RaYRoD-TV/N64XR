# N64-in-VR Standalone Fork — Architecture & Execution Plan

**Status:** plan, pre-bootstrap
**Date:** 2026-06-23

---

## 1. Project Name + Repo Location

**Name: `N64XR`** (working title; alternatives in §10).

**Disk location:** `C:\Users\RaYRoD\Documents\Projects\N64XR\` — a self-contained project tree with its own git repo and toolchain.

**Why a top-level `Projects\` folder:**
- Git-based from day one (see §2).
- Self-contained MSVC + Qt6 + Ninja toolchain, isolated from any other project.
- Leaves room for other `Projects\<other>\` projects later.

**Inside the repo:**
```
N64XR\
  cmake\                       custom find modules, toolchain files
  vendor\                      git submodules (see §2)
    mupen64plus-core\
    GLideN64\
    parallel-rsp\
    parallel-rdp-standalone\
  src\
    frontend\                  Qt6 GUI (forked simple64-gui)
    plugins\
      gfx-vr\                  the new OpenXR-backed GFX plugin
      audio-spatial\           Steam Audio audio plugin
      input-xr\                OpenXR motion-controller input plugin
    common\                    shared headers (ABI shims, IPC, logging)
    vr-scene\                  CRT room scene, models, materials
  shaders\                     GLSL/SPIR-V sources (gfx-vr backend)
  assets\                      CRT mesh, room mesh, IPL probe data
  third_party_licenses\        text files for compliance
  docs\
  LICENSE                      GPL-3.0
  NOTICE                       attributions
  README.md
  CMakeLists.txt
```

---

## 2. Upstream Strategy

**Approach: wrapper repo + git submodules + clean patch overlay.**

Concretely:
- `N64XR` (the new repo, **GPL-3.0**) is the wrapper. It owns: the frontend fork, all three new plugins, CMake build orchestration, shaders, VR scene, docs.
- `vendor/mupen64plus-core` = submodule pointing at **my own GitHub mirror** of `mupen64plus/mupen64plus-core` (the live upstream — *not* simple64's archived fork; the live upstream keeps getting security fixes).
- `vendor/GLideN64` = submodule pointing at **my own fork** of `gonetz/GLideN64`. This one gets modified (Phase 1b stereo injection, §5). Track upstream via periodic rebase.
- `vendor/parallel-rdp-standalone` = submodule of upstream (MIT — keep clean, don't fork unless needed).
- `vendor/parallel-rsp` = submodule of upstream.
- **simple64's frontend** = copied (not submoduled) into `src/frontend/`. Reasons: (a) it's archived dead code, (b) ~40% of it gets rewritten for VR menus + room config, (c) submoduling a dead repo just to fork half of it is overhead with no benefit.

| Strategy | Pros | Cons | Verdict |
|---|---|---|---|
| Full GitHub fork of simple64 (monorepo) | One repo to clone | Inherits dead upstream, hard to track 3 live submodules separately, license noise | NO |
| Vendor as git submodules only | Clean separation, easy upstream rebase | Need a wrapper anyway for the new plugins | partial |
| **Wrapper repo + submodules + patch overlay** | Live upstreams stay rebasable; my code lives in my repo with clean history; the license tree is obvious to auditors | Slightly more CMake plumbing | **YES** |

**Submodule mirror protocol:** before `git submodule add`, fork the live upstream into my own GitHub account and point the submodule at *my mirror*. simple64's archival is the cautionary tale — if upstream disappears, submodule fetches break. Mirror first, fork second.

---

## 3. Architecture Diagram

```
                       ┌─────────────────────────────────────────────┐
                       │  N64XR.exe  (Qt6 frontend, GPL-3.0)  │
                       │  - ROM browser  - VR room launcher          │
                       │  - per-game profile editor (Phase 1b)       │
                       └────────────────────┬────────────────────────┘
                                            │ loads via M64P plugin ABI
                       ┌────────────────────▼────────────────────────┐
                       │  mupen64plus-core.dll  (GPL-2.0, unchanged) │
                       │  - CPU dynarec, RDRAM, MI, AI, VI, SI       │
                       │  - dispatches to four C-ABI plugins:        │
                       └─┬──────┬──────────┬──────────┬──────────────┘
                         │      │          │          │
              ┌──────────▼──┐ ┌─▼────────┐ │ ┌────────▼───────────┐
              │ RSP plugin  │ │ INPUT    │ │ │ AUDIO plugin       │
              │ parallel-rsp│ │ input-xr │ │ │ audio-spatial.dll  │
              │ (MIT)       │ │ .dll     │ │ │ (new, BSD/MIT)     │
              └──────────┬──┘ └──┬───────┘ │ └────┬───────────────┘
                         │       │ XR poses│      │ 16-bit BE PCM
                         │       │ buttons │      │  ↓ swap + resample
                         │       │ haptics │      │  ↓ shm ring → Steam Audio
                         ▼       ▼         ▼      ▼  ↓ HRTF + occlusion
                       (RDP cmd FIFO)              ▼ XrAudio device → HMD
                         │
              ┌──────────▼────────────────────────────────────┐
              │  GFX plugin: gfx-vr.dll  (GPL-2.0, forked)    │
              │  ┌─────────────────────────────────────────┐  │
              │  │ GLideN64 RDP→triangle/state translator  │  │
              │  │  (modified: stereo hook in             │  │
              │  │   _gSPCombineMatrices for Phase 1b)     │  │
              │  └────────────────────┬────────────────────┘  │
              │                       ▼                       │
              │  ┌─────────────────────────────────────────┐  │
              │  │ Vulkan backend (replaces opengl_*.cpp)  │  │
              │  │ - VMA allocator                         │  │
              │  │ - VK_KHR_multiview render pass          │  │
              │  │ - SPIR-V shader gen via glslang         │  │
              │  └────────────────────┬────────────────────┘  │
              └───────────────────────┼────────────────────────┘
                                      │ VkImage swapchain
                                      ▼
              ┌─────────────────────────────────────────────────┐
              │  VR runtime layer  (vr-scene module)            │
              │  ─────────────────────────────────────────────  │
              │  Phase 1a:  N64 framebuffer → texture →         │
              │             rendered onto CRT mesh inside       │
              │             a small room scene (Vulkan)         │
              │                                                 │
              │  Phase 1b:  per-eye VkImages from gfx-vr →      │
              │             submitted directly as projection    │
              │             layers to OpenXR compositor.        │
              │             HUD ortho draws → quad layer        │
              │                                                 │
              │  OpenXR via XR_KHR_vulkan_enable2               │
              │  - xrWaitFrame / xrBeginFrame pacing            │
              │  - quad layers for HUD + menus                  │
              │  - cylinder layer fallback for sub-refresh src  │
              └────────────────────────┬────────────────────────┘
                                       │ xrEndFrame
                                       ▼
              ┌─────────────────────────────────────────────────┐
              │ OpenXR Runtime (SteamVR / Oculus PC / Pimax /   │
              │ Quest Link / native Quest 3 standalone later)   │
              └────────────────────────┬────────────────────────┘
                                       ▼
                              Headset (RTX 5090 → HMD)
```

---

## 4. Phase 1a — Virtual CRT in Room

**Estimated duration: 6–8 weeks** (solo, evenings + weekends).

### 4.1 Goals + Acceptance Criteria
**Goal:** Put on the Quest 3 (over Link) or Pimax Dream Air, launch `N64XR.exe`, pick an N64 ROM, and find yourself sitting in a small room with a CRT TV on a stand, the game running on the CRT with sound coming from the TV's speakers (spatialized). You play with motion controllers that show as a held N64 controller model. No stereo injection yet — the game looks exactly like it would on a real TV.

**Acceptance criteria (PASS signals):**
1. Mario 64 boots; title screen visible on the CRT mesh.
2. Audio is spatialized — walking around the CRT shifts L/R; standing behind a wall mutes it.
3. Right thumbstick = analog stick; A/B = N64 A/B; right-grip-held + face buttons = C buttons; the full input mapping (§4.2 E) works.
4. 90 Hz steady on RTX 5090 + Quest 3 over Link (the room is trivial — anything less is a bug).
5. Recenter on long-press menu button (2s).
6. Closing the ROM returns to the Qt frontend cleanly (no process leaks).
7. Cold-launch to "game running on CRT" in < 15 seconds.

### 4.2 Workstreams + Tasks

#### A. Build system + bootstrap (week 1)
| Task | Size |
|---|---|
| CMake top-level + submodule wiring | 2d |
| MSVC build of mupen64plus-core (non-trivial — GCC-isms in the dynarec; budget extra) | 5d |
| MSVC build of GLideN64 (already mostly portable) | 2d |
| MSVC build of parallel-rdp-standalone (MIT, Vulkan, clean) | 1d |
| simple64-gui copy + MSVC build (Qt6) | 3d |

**Risk:** the mupen64plus-core MSVC port may force a fallback to MSYS2 MinGW64 for the core only, with MSVC for plugins. Decision gate at end of week 1.

#### B. Vulkan backend for GLideN64 (weeks 2–4)
| Task | Size |
|---|---|
| Vulkan device + VMA + queue setup in a new `Graphics/VulkanContext/` tree (mirror `OpenGLContext/`) | 3d |
| FBO equivalent → `VkRenderPass` + `VkFramebuffer` | 4d |
| Texture cache → `VkImage` + descriptor sets | 3d |
| Shader generator: GLSL → glslang → SPIR-V at runtime | 5d |
| Combiner program builder Vulkan path | 5d |
| BufferedDrawer Vulkan path (replace `glDrawRangeElementsBaseVertex`) | 3d |
| **Reference parallel-rdp-standalone's Vulkan setup heavily** | — |

#### C. OpenXR + Vulkan scene host (weeks 2–4, parallel to B)
| Task | Size |
|---|---|
| OpenXR instance/session/swapchain skeleton (Khronos `hello_xr` as the starting scaffold) | 3d |
| Vulkan render of a known-good scene (1 cube) in stereo at 90 Hz | 2d |
| Room mesh + CRT mesh loading (glTF via tinygltf, no UE here) | 3d |
| Material/shader for the CRT (emissive texture sampled from gfx-vr output) | 3d |
| Texture handoff: gfx-vr Vulkan framebuffer → CRT material (shared `VkImage` + semaphore) | 4d |

#### D. Audio plugin `audio-spatial` (week 5)
| Task | Size |
|---|---|
| Fork `mupen64plus-audio-sdl` as a template; MSVC build | 1d |
| BE→LE byte swap + int16→float32 + resample (libsamplerate) | 2d |
| **No UE integration** — Steam Audio's C API directly (we're standalone) | 1d |
| Steam Audio HRTF + occlusion in-process (geometry = room collision baked at room-load) | 4d |
| Audio device → OpenXR audio sink (or fall back to WASAPI) | 2d |

#### E. Input plugin `input-xr` (week 5–6)
| Task | Size |
|---|---|
| Fork `simple64-input-qt` as a template | 1d |
| OpenXR action set: thumbsticks, A/B, grip, triggers, menu, haptic | 3d |
| Concrete mapping (Phase 1a default — see below) | 2d |
| Held-right-grip modifier for C-buttons (load-bearing) | 1d |
| Haptic on rumble-pak callback | 1d |

**Default input map:** left stick → analog; A/B → A/B; L/R trigger → Z/R; left grip → L; menu → Start; right stick (or held-right-grip + face buttons) → C-buttons.

#### F. CRT room scene + assets (weeks 4–6, parallel)
| Task | Size |
|---|---|
| Source/buy/model a 1996-vintage CRT mesh (CC0 or commercial-friendly) | 2d |
| Small room mesh (study, basement, attic) — golden-age sci-fi adjacent | 3d |
| Material: phosphor scanlines + slight barrel-distortion shader | 3d |
| Lighting bake (Steam Audio probes need the same geometry — share the mesh) | 2d |
| Controller model: stylized N64 pad held in an IK hand pose | 2d |

#### G. Settings UI in VR (week 6–7)
| Task | Size |
|---|---|
| Diegetic clipboard model + raycast pointer | 2d |
| ROM browser as a stack of cartridges on a shelf | 3d |
| Rebind UI listening for the next physical input within 5s | 2d |
| JSON profile load/save under `%APPDATA%\N64XR\InputProfiles\` | 1d |

### 4.3 Critical Path
```
Build bootstrap → Vulkan-GLideN64 backend ─┐
                                           ├→ Texture handoff → CRT-in-room demo → 1a DONE
            OpenXR + scene host ───────────┘
                          ↑
        Audio plugin ─────┘   (parallel, gates demo polish, not boot)
        Input plugin ─────┘
```

**Longest leg = Vulkan-GLideN64 backend (3 weeks).** Everything else can be parallelized.

### 4.4 Definition of Done — Phase 1a Ship Day
You put on the headset, see a softly-lit attic room with a CRT on a wooden TV stand. A shelf of cartridges sits to your right. You grab Super Mario 64, slot it into the console below the CRT (animated insert), the screen flickers on with the SGI logo, and the title music plays from the TV. You pick up the controller from the table — it follows your right hand. Hold-grip + face buttons selects File A. You play in front of the TV for an hour without sickness, then long-press Menu to recenter when you lean back in your chair.

---

## 5. Phase 1b — Stereo Injection

**Estimated duration: 10–14 weeks after 1a ships.**

### 5.1 Goals + Acceptance Criteria
**Goal:** the same launcher now exposes a "Stereo VR" toggle per game. Toggling it on Mario 64 makes the castle exist in 3D space around the player; turning your head looks around (within the rendered frustum), and the HUD floats at a comfortable distance as a positioned quad.

**Acceptance criteria:**
1. The SM64 castle renders with real geometric depth (verify in a RenderDoc capture — two distinct view matrices, not a screen-space depth fake).
2. The HUD (power meter, coin count, star count) renders as a single billboarded quad layer.
3. No double-vision artifacts on framebuffer effects (lens-of-truth shimmer, motion-blur paths force-mono).
4. Per-game profile JSON ships for SM64, Mario Kart 64, OoT, Banjo-Kazooie, GoldenEye, Star Fox 64.
5. Unknown games fall back to Phase 1a CRT mode automatically.
6. Frame pace clean on RTX 5090 + Quest 3 Link at 90 Hz (multiview enabled).

### 5.2 Workstreams + Tasks

#### A. Stereo matrix injection in GLideN64 (weeks 1–3)
| Task | Size |
|---|---|
| Add a `gSP.stereo` state struct (per-eye view-offset matrix, IPD) | 1d |
| Hook `_gSPCombineMatrices` in `src/gSP.cpp` to compute `combined[2]` | 2d |
| Duplicate `gSPProcessVertex` / `gSPTransformVertex` for two output buffers | 3d |
| Mirror the NEON_OPT path (or `#ifdef` it out for desktop — N/A on x64) | 1d |
| Patch `drawTriangles` / `drawDMATriangles` to dispatch both eye buffers | 3d |
| Force mono in `drawRect`, `drawTexturedRect`, `drawScreenSpaceTriangle` | 2d |
| Projection-shape heuristic safety net for 3D-call HUDs (M[2][3] ≈ 0 → ortho → mono) | 2d |
| `VK_KHR_multiview` render pass — single pass, two views | 4d |

#### B. HUD-to-world-quad mechanism (weeks 3–5)
**Detection:** classify every draw call by:
1. **Type signal** (best): `drawRect` + `drawTexturedRect` + `drawScreenSpaceTriangle` → HUD candidates, never stereo-projected.
2. **Matrix shape** (fallback for `drawTriangles` cases): inspect `gSP.matrix.projection`; if `M[3][3] == 1 && M[2][3] == 0` → ortho → HUD.
3. **Per-game override** (final word): profile JSON lists "always HUD" / "never HUD" draw signatures.

**Mechanism:**
- Render all HUD candidates into a separate `VkImage` (HUD render target) at native N64 resolution × 4 upscale.
- Submit as an OpenXR **quad composition layer** anchored 1.5m forward, sized 1.6m wide, billboarded to head yaw (not full headlock — head-locked HUDs are nauseating).
- The user can grab the quad with a controller and reposition/resize it. Position is stored in the profile JSON.

#### C. Per-game profile system (week 2, parallel)
**Format: JSON** (human-readable, diffable, community-shareable).

Path: `%APPDATA%\N64XR\GameProfiles\<rom_crc32>.json`

Schema:
```json
{
  "rom_id": "SUPER MARIO 64",
  "rom_crc32": "635A2BFF",
  "display_name": "Super Mario 64",
  "stereo": {
    "enabled": true,
    "ipd_scale": 1.0,
    "world_scale": 0.05,
    "near_clip": 1.0,
    "far_clip": 100000.0,
    "force_mono_draws": ["framebuffer_blit", "lens_distortion"]
  },
  "hud": {
    "mode": "quad_layer",
    "anchor": "head_yaw",
    "distance_m": 1.5,
    "width_m": 1.6,
    "always_hud_signatures": ["tex_rect:0x80300000"],
    "never_hud_signatures": []
  },
  "input": {
    "archetype": "3d_platformer",
    "overrides": {}
  },
  "camera": {
    "hook_type": "sm64_lakitu",
    "decouple_head_from_camera": true
  }
}
```

#### D. Per-game camera hooks (weeks 5–10, one title per ~5 days)

The honest reality: **there is no general-purpose camera hook for arbitrary N64 games.** The approach:
- **Tier 1 — works on any game without code:** stereo matrix injection alone (§5.2 A). Head turns within the rendered frustum only. Some games show black voids when you turn too far. Acceptable for the Phase 1b ship.
- **Tier 2 — per-title:** RAM-watch hooks that read the game's camera variables at known addresses and let the head pose nudge them. Requires reverse-engineering each title.

**Priority order:**
1. **Super Mario 64** — well-mapped (TASvideos, decomp). Camera at a known RAM offset. Sets the template.
2. **Mario Kart 64** — cockpit-cam mod feasible; first-person karting is a known crowd-pleaser.
3. **Ocarina of Time** — most-requested. Decoupled first-person camera (like Kaze's mod, but without his frustum-cull bug since we're emulating, not patching the ROM).
4. **Banjo-Kazooie** — similar pattern to SM64.
5. **GoldenEye 007** — already first-person; aim-with-controller injection (archetype_fps).
6. **Star Fox 64** — cockpit cam.

Per title: 3–5 days of RAM-watching with a debugger + writing the hook + tuning.

#### E. Frame pacing + multiview polish (weeks 10–12)
- Validate that single-pass multiview saves the CPU/GPU it claims to.
- Cylinder-layer fallback when source FPS < headset refresh.
- ETFR plumbing on Quest 3 / Pimax Dream Air.

### 5.3 Definition of Done — Phase 1b Ship
You boot SM64 with stereo enabled. You're standing on the castle grounds at human scale (world_scale tuned so Mario is ~0.3m tall — diorama-ish; first-person at Mario-scale induces vertigo). The HUD floats comfortably. You run the same test on GoldenEye with archetype_fps and motion-aim — Facility plays like a competent VR shooter. Mario Kart 64 with the cockpit-cam profile feels like driving a kart.

---

## 6. Bootstrap TODO (do this BEFORE coding starts)

**All steps run on the Falcon NW box, in this order. Do not skip.**

1. **Install MSVC 2022 (17.10+)** — Visual Studio 2022 Community or Professional with the **"Desktop development with C++"** workload. Include: MSVC v143 x64/x86 build tools, Windows 11 SDK (10.0.26100 or newer), C++ CMake tools for Windows, C++ Clang tools for Windows (used by some Vulkan SDK utilities). Default install path is fine.

2. **Install Qt 6.7 LTS via the Qt Online Installer** — from `qt.io/download-qt-installer`. Pick: Qt 6.7.x → **MSVC 2022 64-bit**, plus **Qt Creator** (latest), plus **Qt Debug Information Files**. Install to `C:\Qt\` (default). Add `C:\Qt\6.7.x\msvc2022_64\bin` to PATH.

3. **Install vcpkg** in `C:\vcpkg\`:
   ```
   git clone https://github.com/microsoft/vcpkg.git C:\vcpkg
   cd C:\vcpkg
   .\bootstrap-vcpkg.bat
   .\vcpkg integrate install
   ```
   Then install dependencies (~30 minutes):
   ```
   .\vcpkg install --triplet x64-windows ^
     libsamplerate spdlog fmt nlohmann-json tinygltf glslang spirv-cross ^
     vulkan-memory-allocator zlib
   ```

4. **Install Vulkan SDK 1.3.290+** — from `vulkan.lunarg.com/sdk/home#windows`. Install to `C:\VulkanSDK\1.3.290.0\`. Tick "Vulkan Memory Allocator header" and "GLM headers". Reboot after install (PATH/env vars).

5. **Install OpenXR SDK** — clone the source build, **not** the runtime (runtimes are SteamVR / the Oculus app, already on the box):
   ```
   git clone https://github.com/KhronosGroup/OpenXR-SDK.git C:\dev\OpenXR-SDK
   ```
   No build yet — N64XR's CMake pulls it as needed. **Confirm SteamVR + the Oculus PC app are both installed and report a working OpenXR runtime** (run any SteamVR title successfully).

6. **Install RenderDoc + Nsight Graphics** — RenderDoc from `renderdoc.org`, Nsight Graphics from the NVIDIA Developer site. Both are mandatory for Phase 1b debugging.

7. **Install Git for Windows** — confirm `git --version` ≥ 2.40. Also install **Git LFS** (`git lfs install` once) — the CRT mesh and room assets are LFS-tracked.

8. **Create the GitHub repo** — a **public** repo `N64XR` with `README.md`, `.gitignore` (Visual Studio template), `LICENSE` = GPL-3.0. **Do not** init with code yet. Note the clone URL.

9. **Mirror the upstream repos** (fork each into the same account):
   - `mupen64plus/mupen64plus-core` → `RaYRoD/mupen64plus-core`
   - `gonetz/GLideN64` → `RaYRoD/GLideN64`
   - `Themaister/parallel-rdp-standalone` → `RaYRoD/parallel-rdp-standalone`
   - `simple64/parallel-rsp` → `RaYRoD/parallel-rsp`
   - `simple64/simple64-gui` → `RaYRoD/simple64-gui` (read-only reference; copy from this, don't submodule)
   - `simple64/simple64-input-qt` → `RaYRoD/simple64-input-qt` (template)
   - `simple64/simple64-audio-sdl2` → `RaYRoD/simple64-audio-sdl2` (template)

10. **Make the directory + clone:**
    ```
    mkdir C:\Users\RaYRoD\Documents\Projects
    cd C:\Users\RaYRoD\Documents\Projects
    git clone <N64XR clone URL> N64XR
    cd N64XR
    ```

11. **Install the Steam Audio SDK** — download the latest from `valvesoftware.github.io/steam-audio/`. Extract the C API zip to `C:\SDK\steamaudio\`. (Vendored binary dep, not a submodule — it's a fat SDK with prebuilt binaries.)

12. **Buy/source a CRT mesh + room mesh** (or commission one) — needs a commercial-use-OK license. Sketchfab CC-BY filter or Fab.com. Place in `assets/_pending/` until the Phase 1a F-workstream picks them up. **Optional — can defer to week 4.**

13. **Confirm before coding:** (a) the clone URL of the N64XR repo, (b) install paths for Qt + Vulkan + Steam Audio if different from above, (c) which VR runtime to target first for testing (SteamVR via Pimax Dream Air, or Oculus PC via Quest 3 Link). **Then step 1 of §7 begins.**

---

## 7. First-Week Implementation Plan (after bootstrap)

Five concrete starter tasks, in dependency order:

1. **`N64XR/CMakeLists.txt`** + **`N64XR/cmake/N64XRConfig.cmake`** — top-level CMake that:
   - Sets MSVC + C++20.
   - Finds Qt6, Vulkan, OpenXR-SDK, Steam Audio, vcpkg deps.
   - Declares `add_subdirectory()` for each submodule and each `src/` target.
   - Defines export targets `N64XR::Frontend`, `N64XR::GfxVR`, `N64XR::AudioSpatial`, `N64XR::InputXR`.

   **Contribution:** unblocks every other build.

2. **`N64XR/.gitmodules`** + the initial submodule add for `vendor/mupen64plus-core` only, then **build it standalone** via a `vendor/mupen64plus-core/CMakeLists.txt` MSVC patch in `cmake/patches/mupen64plus-core-msvc.patch` if needed.

   **Contribution:** validates the riskiest single bet (the MSVC port of the core). PASS = `mupen64plus-core.dll` builds. FAIL = decide the MSYS2 fallback now, not in week 4.

3. **`N64XR/src/plugins/gfx-vr/src/Plugin.cpp`** — implements the five required M64P exports (`PluginStartup`, `PluginShutdown`, `PluginGetVersion`, `InitiateGFX`, plus the GFX callbacks `ProcessDList`, `UpdateScreen`, `ChangeWindow`, etc.) **as no-op stubs that just log.** Loads cleanly into mupen64plus-core. **No actual rendering yet.**

   **Contribution:** proves the plugin ABI handshake works end-to-end before any Vulkan code exists.

4. **`N64XR/src/vr-scene/src/XrSession.cpp`** + **`XrSession.h`** — a minimal OpenXR session that:
   - Creates an `XrInstance` with `XR_KHR_vulkan_enable2`.
   - Creates `XrSystemId`, `XrSession`, two-eye `XrSwapchain` of format `VK_FORMAT_R8G8B8A8_SRGB`.
   - Runs an `xrBeginFrame`/`xrEndFrame` loop submitting a magenta clear color.

   **Contribution:** the first thing you see in the headset. PASS = solid magenta in both eyes. Validates the whole OpenXR + Vulkan toolchain in one shot.

5. **`N64XR/src/frontend/src/MainWindow.cpp`** (forked from `simple64-gui`) — strip everything except: the window opens, an "Open ROM" button, and a "Launch VR" button. The Launch button calls the gfx-vr plugin's init function (which calls into the §7.4 XrSession).

   **Contribution:** the end-to-end shell. ROM button → core boots → gfx-vr plugin logs that it got called → XrSession opens → magenta in the HMD.

**After these five land:** the next sprint replaces (4)'s magenta clear with a textured CRT mesh, and replaces (3)'s no-op `UpdateScreen` with an actual GLideN64-derived Vulkan render of the N64 framebuffer into a texture sampled by the CRT material.

---

## 8. Risks + Mitigations

| # | Risk | Likelihood | Impact | Mitigation |
|---|---|---|---|---|
| 1 | **The mupen64plus-core MSVC port is harder than expected** (GCC-isms, computed gotos, inline asm in the dynarec) | High | Cripples the timeline | Spike in week 1 (§7.2). If hard-stuck >5d, fall back to MSYS2 MinGW64 for `mupen64plus-core.dll` only; plugins + frontend stay MSVC. The plugin ABI is C, so a cross-compiler is fine. |
| 2 | **The GLideN64 → Vulkan port balloons past 3 weeks** | Medium-High | Phase 1a slips a month | Use parallel-rdp-standalone as a reference; consider a hybrid Phase 1a where GLideN64 stays GL and we GL→Vulkan-interop-blit into the CRT texture. Ship 1a on the hybrid, port to native Vulkan during Phase 1b prep. |
| 3 | **OpenXR + Vulkan + GLideN64 + Steam Audio integration hell** (everything fails at once on first integration) | Medium | Week-long debug spirals | Bring up each leg in isolation first (§7 ordering). Each starter task PASSes independently before integrating. RenderDoc + Nsight on day 1. |
| 4 | **Per-game camera hooks (Phase 1b) are an infinite project** | Certain | Phase 1b never "done" | Ship Phase 1b with **only Tier-1 stereo matrix injection** for all titles + hand-tuned profiles for the 6 priority games. Anything beyond that is post-1b community-contributable JSON. Define done aggressively. |
| 5 | **Frustum-culling void when the head turns** | High | Phase 1b feels broken on first-person titles | Phase 1b ships **third-person + diorama-scaled by default.** First-person profiles ship later, per-game, with disabled-far-clip workarounds where the game's vertex pipeline allows it. Set the expectation now. |

---

## 9. Licensing Checklist

**Files in the repo root:**
- `LICENSE` — **GPL-3.0** full text. (New code; covers the wrapper.)
- `NOTICE` — top-level attribution summary (3–5 lines per upstream).
- `README.md` — what it is, link to the source-offer URL (the repo itself; GPLv3 §6 "equivalent access" is satisfied by a public GitHub release with matching tagged source).

**Files in `third_party_licenses/`:**
- `mupen64plus-core.LICENSE.txt` (GPL-2.0 full text)
- `GLideN64.LICENSE.txt` (GPL-2.0)
- `parallel-rdp-standalone.LICENSE.txt` (MIT)
- `parallel-rsp.LICENSE.txt` (MIT)
- `Qt6.LICENSE.txt` (LGPL-3.0) — **with an explicit Qt-version note + relink instructions per LGPLv3 §4**
- `OpenXR-SDK.LICENSE.txt` (**MIT option chosen** — note explicitly in NOTICE)
- `Vulkan-Loader.LICENSE.txt` (Apache-2.0)
- `Vulkan-Memory-Allocator.LICENSE.txt` (MIT)
- `glslang.LICENSE.txt` (BSD-3 + Apache-2.0)
- `SteamAudio.LICENSE.txt` (Apache-2.0)
- `libsamplerate.LICENSE.txt` (BSD-2)
- `nlohmann-json.LICENSE.txt` (MIT)
- `tinygltf.LICENSE.txt` (MIT)

**`NOTICE` template:**
```
N64XR — an N64-in-VR experience
Copyright (C) 2026 Ray Rod

This is an UNOFFICIAL fork, not endorsed by:
  - the Mupen64Plus team
  - the GLideN64 maintainers (Sergey Lipskiy et al.)
  - loganmc10 (simple64, archived 2026-02-14)

Licensed under GPL-3.0. See LICENSE.

Bundled components:
  - Mupen64Plus core              GPL-2.0   github.com/mupen64plus/mupen64plus-core
  - GLideN64 (modified)            GPL-2.0   github.com/gonetz/GLideN64
  - parallel-rdp-standalone        MIT       github.com/Themaister/parallel-rdp-standalone
  - parallel-rsp                   MIT       github.com/simple64/parallel-rsp
  - Frontend code derived from simple64-gui (GPL-3.0)
  - Qt 6.7 LTS                     LGPL-3.0  (dynamically linked; see third_party_licenses/Qt6.LICENSE.txt)
  - OpenXR SDK (MIT option)        MIT       github.com/KhronosGroup/OpenXR-SDK
  - Vulkan headers + loader        Apache-2.0
  - Steam Audio                    Apache-2.0
  - (full list in third_party_licenses/)

Source: https://github.com/RaYRoD/N64XR
Release tags match shipped binaries 1:1 (GPLv3 §6).
```

**Rules to enforce:**
- **Never bundle the Meta XR Audio SDK.** OpenXR + Steam Audio only.
- **Never strip upstream copyright headers** when patching GLideN64 / mupen64plus-core.
- **Every binary release** = matching `git tag` + GitHub release with a source zip + the `LICENSE` + `NOTICE` + `third_party_licenses/` folder included.
- **No EULA, no telemetry, no "non-commercial" rider.** GPL forbids further restrictions.

---

## 10. Naming + Branding

**Choice: `N64XR`**

**Why:**
- "XR" — broad enough for OpenXR / VR / future passthrough-MR room modes without locking into "VR" only.
- Short enough for a window title, pronounceable.
- No collision with any active emulator project found.
- Reads like a *product*, not a fork — supports an eventual Reddit/HN announcement when 1a ships.

**Runner-ups:**
- `Stereo64` — too narrow (locks out passthrough/MR plans).
- `RetroXR` — too generic, probably already used.

**Window title format:** `N64XR — Super Mario 64 (USA) — Stereo` (game + mode).

**Logo direction (if any):** a stylized N64 cartridge silhouette inside a hexagon (OpenXR's house mark is a hex). Not load-bearing for 1a — defer.

---

**End of plan.** Review §1, §2, §6, §10 first — those are the irreversible early decisions.
