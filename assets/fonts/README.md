# N64XR — Fonts

Three SIL OFL 1.1 typefaces, vendored in this directory. Shipped with the binary; no separate attribution screen needed beyond `LICENSES.md`.

| Role | Family | Files | Upstream |
|---|---|---|---|
| Body / HUD | JetBrains Mono | `JetBrainsMono-Regular.ttf`, `JetBrainsMono-Medium.ttf` | [github.com/JetBrains/JetBrainsMono](https://github.com/JetBrains/JetBrainsMono) — release v2.304 |
| Display / titling | Orbitron | `Orbitron-Regular.ttf`, `Orbitron-Bold.ttf` | [github.com/google/fonts/tree/main/ofl/orbitron](https://github.com/google/fonts/tree/main/ofl/orbitron) |
| Phosphor status | VT323 | `VT323-Regular.ttf` | [github.com/google/fonts/tree/main/ofl/vt323](https://github.com/google/fonts/tree/main/ofl/vt323) |

The root `CMakeLists.txt` mirrors `assets/` next to the frontend exe at build time (`n64xr_copy_assets` target), so the launcher finds them on first run.

## Swapping a face

Drop the replacement `.ttf` over the existing file (same filename). The launcher hot-resolves at startup; if a file is missing it logs a `warn` and falls back to ImGui's built-in ProggyClean for that slot. Mix-and-match within one OFL/permissive family is fine — re-attribute in `LICENSES.md` if you change upstream.
