// ============================================================================
//  Icons.h — crisp hand-drawn vector icons + HUD-chrome helpers (ImDrawList).
// ----------------------------------------------------------------------------
//  No fonts, no images. Every icon draws inside a square box [center-r,
//  center+r], so they are resolution-independent and trivially scalable.
//  Line-style (transparent-safe). Shared signature: (dl, center, r, col, thick).
//  Also exposes HUD frame chrome: octagonal cut-corner frame, animated corner
//  brackets, and a receding perspective floor grid.
// ============================================================================
#pragma once

#include <imgui.h>

namespace n64xr::ui::icons {

// ---- Nav-bar glyphs (line-style) ------------------------------------------
void DrawGamesGridIcon (ImDrawList* dl, ImVec2 c, float r, ImU32 col, float thick = 2.0f);
void DrawGearIcon      (ImDrawList* dl, ImVec2 c, float r, ImU32 col, float thick = 2.0f);
void DrawGamepadIcon   (ImDrawList* dl, ImVec2 c, float r, ImU32 col, float thick = 2.0f);
void DrawVRHeadsetIcon (ImDrawList* dl, ImVec2 c, float r, ImU32 col, float thick = 2.0f);
void DrawCartridgeIcon (ImDrawList* dl, ImVec2 c, float r, ImU32 col, float thick = 2.0f);

using IconFn = void(*)(ImDrawList*, ImVec2, float, ImU32, float);

// ---- HUD chrome -----------------------------------------------------------
void HoloFrame(ImDrawList* dl, ImVec2 a, ImVec2 b, float cut, ImU32 col, float thick);
void HoloFrameGlow(ImDrawList* dl, ImVec2 a, ImVec2 b, float cut, ImU32 core,
                   int rings = 4, float spread = 2.0f);
void CornerBrackets(ImDrawList* dl, ImVec2 a, ImVec2 b, float baseLen,
                    ImU32 col, float time);
void PerspectiveGrid(ImDrawList* dl, ImVec2 origin, float w, float h,
                     ImU32 col, float scrollT);

} // namespace n64xr::ui::icons
