// ============================================================================
//  GlassPanel.h — frosted-glass instrument-panel chrome (pure ImDrawList).
// ----------------------------------------------------------------------------
//  No Vulkan. Layered translucency + luminance gradient + grain + border glow
//  so the 3D cartridge behind reads through as soft colour. Z-order per panel:
//    shadow -> frosted fill -> top sheen -> grain -> inner/outer borders ->
//    edge glow -> (scrim + tracked text are separate helpers).
// ============================================================================
#pragma once

#include <imgui.h>

namespace n64xr::ui::glass {

// Draw the full frosted panel body into [a,b] on drawlist `dl`. `accent` is
// the border-glow colour (brass for frames, phosphor for active readouts).
// `tint` is the frosted fill colour (cool navy by default). Call once per
// panel BEFORE you emit its content.
void Panel(ImDrawList* dl, ImVec2 a, ImVec2 b, float rounding,
           ImU32 tint, ImU32 accent, float accentStrength = 1.0f);

// Soft dark wash behind a text cluster so it stays legible over the moving 3D
// area. Opaque at top, fades to nothing at the bottom.
void Scrim(ImDrawList* dl, ImVec2 a, ImVec2 b, ImU32 dark = IM_COL32(4, 7, 14, 150));

// Glow ring around [a,b] — several expanding low-alpha rects accumulate toward
// the accent colour over a dark scene (fake additive). Drawn by Panel(); also
// exposed for stand-alone use (e.g. the hero button frame).
void EdgeGlow(ImDrawList* dl, ImVec2 a, ImVec2 b, float rounding,
              ImU32 accent, int rings = 5, float spread = 1.6f);

// Tracked (letter-spaced) text rendered glyph-by-glyph at `px`. Returns the
// advance width so callers can centre / right-align. Draws a 1px shadow first.
float TextTracked(ImDrawList* dl, ImFont* font, float px, ImVec2 pos,
                  ImU32 col, const char* text, float tracking);

// Width-only measure of a tracked string (no draw) for centering.
float MeasureTracked(ImFont* font, float px, const char* text, float tracking);

} // namespace n64xr::ui::glass
