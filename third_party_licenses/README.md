# Third-Party Licenses

Each bundled component's full license text lives under this directory. N64XR is GPL-3.0-or-later (see `../LICENSE`); the components keep their original terms.

## Component → license file mapping

| Component | License | File | Source |
|---|---|---|---|
| Mupen64Plus core | GPL-2.0 | `mupen64plus-core.LICENSE.txt` | `vendor/mupen64plus-core/LICENSES/` (submodule) |
| GLideN64 (modified) | GPL-2.0 | `GLideN64.LICENSE.txt` | `vendor/GLideN64/LICENSE` |
| parallel-rdp-standalone | MIT | `parallel-rdp-standalone.LICENSE.txt` | `vendor/parallel-rdp-standalone/LICENSE.MIT` |
| parallel-rsp | MIT | `parallel-rsp.LICENSE.txt` | `vendor/parallel-rsp/LICENSE` |
| simple64-gui (frontend derived) | GPL-3.0 | `simple64-gui.LICENSE.txt` | upstream `simple64/simple64-gui/LICENSE` |
| Qt 6.7 LTS | LGPL-3.0 | `Qt6.LICENSE.txt` | Qt installer |
| OpenXR SDK | MIT (chosen) | `OpenXR-SDK.LICENSE.txt` | `C:/dev/OpenXR-SDK/LICENSE` |
| Vulkan Loader | Apache-2.0 | `Vulkan-Loader.LICENSE.txt` | LunarG SDK |
| Vulkan Memory Allocator | MIT | `Vulkan-Memory-Allocator.LICENSE.txt` | vcpkg port |
| glslang | BSD-3 + Apache-2.0 | `glslang.LICENSE.txt` | vcpkg port |
| Steam Audio | Apache-2.0 | `SteamAudio.LICENSE.txt` | `C:/SDK/steamaudio/THIRDPARTY.md` |
| libsamplerate | BSD-2 | `libsamplerate.LICENSE.txt` | vcpkg port |
| nlohmann/json | MIT | `nlohmann-json.LICENSE.txt` | vcpkg port |
| tinygltf | MIT | `tinygltf.LICENSE.txt` | vcpkg port |
| spdlog | MIT | `spdlog.LICENSE.txt` | vcpkg port |
| fmt | MIT | `fmt.LICENSE.txt` | vcpkg port |
| zlib | zlib | `zlib.LICENSE.txt` | vcpkg port |

## How the build keeps this current

A CMake target `n64xr_collect_licenses` copies the source-of-truth license files from the submodules and the local SDK installs into this directory at configure time, so every binary release ships a self-contained `third_party_licenses/` folder. (Target lands with the build-system commit; pending for now.)

## Adding a new dependency

1. Check the license is GPL-3.0-compatible. **Never bundle Meta XR Audio SDK or any "non-commercial" / EULA-encumbered library.**
2. Add an entry to the table above with link to upstream LICENSE.
3. Wire it into `n64xr_collect_licenses` in `cmake/CollectLicenses.cmake`.
4. Update root `NOTICE` with a one-line attribution.
