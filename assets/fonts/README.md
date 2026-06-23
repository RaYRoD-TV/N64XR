# N64XR — Font drop-zone

The launcher's theme looks for these files in `assets/fonts/` next to the
exe at runtime.  All three are **SIL OFL** licensed — ship them with the
binary, no separate attribution screen needed beyond a `LICENSES.md` later.

If any of these are missing the launcher will still boot — it falls back to
the built-in ImGui ProggyClean font and logs a `warn`-level line per missing
face.  But you won't get the golden-age-sci-fi look until they're in place.

## TODO — download these in order (most important first)

1. **JetBrains Mono — the body face** (highest priority; every menu line uses it)
   - Download:  https://www.jetbrains.com/lp/mono/  →  click *Download font*
   - From the zip, copy into `assets/fonts/`:
     - `JetBrainsMono-Regular.ttf`
     - `JetBrainsMono-Medium.ttf`  *(optional — unused today, reserved for emphasis)*
   - License:  SIL OFL 1.1

2. **Orbitron — the display/titling face** (screen headers, hero button label, tab spine)
   - Download:  https://fonts.google.com/specimen/Orbitron  →  *Get font* → *Download all*
   - From the zip, copy into `assets/fonts/`:
     - `Orbitron-Regular.ttf`
     - `Orbitron-Bold.ttf`
   - License:  SIL OFL 1.1

3. **VT323 — the phosphor status face** (bottom status strip, "console readout" column)
   - Download:  https://fonts.google.com/specimen/VT323  →  *Get font* → *Download all*
   - From the zip, copy into `assets/fonts/`:
     - `VT323-Regular.ttf`
   - License:  SIL OFL 1.1

## Final layout

```
assets/
  fonts/
    JetBrainsMono-Regular.ttf
    JetBrainsMono-Medium.ttf      (optional)
    Orbitron-Regular.ttf
    Orbitron-Bold.ttf
    VT323-Regular.ttf
    README.md                     (this file)
```

The root `CMakeLists.txt` mirrors `assets/` next to the frontend exe at
build time (`n64xr_copy_assets` target), so you only need to drop the
files in once.
