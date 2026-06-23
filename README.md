# N64 XR

Standalone OpenXR-backed VR fork of [simple64](https://github.com/simple64/simple64) + [GLideN64](https://github.com/gonetz/GLideN64). Brings Nintendo 64 emulation to the headset.

> **Status:** pre-alpha. Phase 1 scaffolding — not yet runnable.

## What it is

Two-phase VR rollout on top of a `mupen64plus-core` + (modified) `GLideN64` stack:

- **Phase 1a — Virtual CRT in OpenXR room.** The N64 framebuffer textures a CRT mesh sitting in a small room. Hand-tracked motion controllers act as a gamepad. Audio spatializes from the TV. Flatscreen-on-virtual-TV experience.
- **Phase 1b — Stereo VR injection.** Per-draw projection matrix replacement in GLideN64's microcode translator for real 6DOF stereo. Per-game camera hooks (starting with Super Mario 64). HUD ortho draws → world-space quad layer.

Full plan: [`docs/plan.md`](docs/plan.md).

## Repo layout

```
cmake/                       find modules, toolchain helpers
vendor/                      git submodules
  mupen64plus-core/          GPL-2.0  CPU dynarec, RDRAM, MI, AI, VI, SI
  GLideN64/                  GPL-2.0  HLE graphics plugin (we modify for stereo)
  parallel-rdp-standalone/   MIT      reference LLE RDP (Vulkan)
  parallel-rsp/              MIT      RSP plugin
src/
  frontend/                  Qt6 GUI (copied/forked from simple64-gui)
  plugins/
    gfx-vr/                  OpenXR-backed graphics plugin
    audio-spatial/           Steam Audio-backed audio plugin
    input-xr/                OpenXR motion-controller input plugin
  common/                    shared headers (ABI shims, IPC, logging)
  vr-scene/                  CRT room scene, models, materials
shaders/                     GLSL → SPIR-V (compiled at build time)
assets/                      CRT mesh, room mesh, IPL probe data
third_party_licenses/        compliance text
docs/                        plan, architecture, design notes
```

## Build (Windows / MSVC 2022+)

Requires:
- Visual Studio 2026 (or 2022 17.10+) — installer workload **Desktop development with C++**
- Qt 6.7 LTS (MSVC 2022 64-bit) — only needed for the frontend target
- Vulkan SDK 1.3.290+ (1.4.350 known-good)
- vcpkg at `C:\vcpkg` (auto-detected by `CMAKE_TOOLCHAIN_FILE`)
- OpenXR-SDK clone at `C:\dev\OpenXR-SDK` (override with `-DOPENXR_SDK_ROOT=...`)
- Steam Audio SDK 4.8.1+ at `C:\SDK\steamaudio` (only needed when `N64XR_BUILD_AUDIO=ON`)

```
git clone --recurse-submodules https://github.com/RaYRoD-TV/N64XR.git
cd N64XR
cmake -B build -S . -G "Visual Studio 18 2026" -A x64 ^
      -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake
cmake --build build --config Release
```

Substitute `-G "Visual Studio 17 2022"` if you're on VS 2022. To skip the Qt-dependent frontend (no Qt installed yet), add `-DN64XR_BUILD_FRONTEND=OFF` and `--target gfx_vr n64xr_vr_scene`.

## License

GPL-3.0-or-later. See [`LICENSE`](LICENSE) and [`NOTICE`](NOTICE). Bundled component licenses under [`third_party_licenses/`](third_party_licenses/).

This is an **unofficial** fork. Not affiliated with Nintendo. No ROMs are distributed; supply your own legally-dumped cartridges.
