# N64XR — cartridge model drop-zone

Drop a Nintendo 64 cartridge model here and the launcher renders **it** as the
holographic centerpiece instead of the built-in procedural shape.

## How

1. Put a model file in this folder named **`cartridge.glb`**
   (any other `.glb` / `.gltf` here also works — `cartridge.glb` wins if present).
2. Rebuild (the build copies `assets/` next to the exe), or just drop the file
   into `build-cli/bin/Debug/assets/models/` next to the running exe.
3. Launch — the console logs `[glTF] cartridge loaded: N tris …`.

The loader auto-centres, orients (smallest dimension → depth, largest → height),
and scales the model to fit, then renders it as a translucent brass hologram
with only the **real edges** lit (creases + silhouette, no triangulation noise)
plus the phosphor sweep and bloom. If no model is here, it falls back to the
procedural cartridge — so this folder can stay empty.

## Where to get one

Free models exist on Sketchfab (search "Nintendo 64 cartridge", filter
*Downloadable*). Note licenses:
- **CC0** — do anything, no attribution. Best.
- **CC-BY** — free to use, just credit the author.
- **CC-BY-ND / -NC** — fine for personal local use, but **don't redistribute**
  a build with the model embedded.

Models here are **git-ignored** — they're yours, not committed to the repo
(which ships the procedural fallback to stay license-clean).

## Tips

- If the cart loads sideways / backwards, tell me and I'll add an orientation
  fix (some models are authored Z-up or facing away).
- Lower-poly models (≤ a few k tris) give the cleanest crease wireframe.
