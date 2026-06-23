// ============================================================================
//  AliveIdle.h — the layer that keeps the UI *breathing* when nobody touches it.
// ----------------------------------------------------------------------------
//  Pure ImDrawList. No Vulkan, no swapchain dependency — so the SAME code
//  paints the desktop launcher AND the in-VR diegetic panel later (the panel
//  just sources a different ImDrawList from a different ImGui context).
//
//  Stacks five cheap layers at different frequencies — the polyrhythm is what
//  reads as "alive," not any single layer:
//      0.16 Hz pane breath  / 0.28 Hz button pulse / 0.7 Hz mote orbit
//      30  Hz glyph scramble (used by typewriter helper)
//      60  Hz scanline roll (frame-locked)
// ============================================================================
#pragma once

#include <imgui.h>
#include <vector>

namespace n64xr::ui::alive {

struct Mote {
    ImVec2 pos;
    ImVec2 vel;
    float  phase = 0.0f;
    float  size  = 1.6f;
};

struct State {
    std::vector<Mote> motes;
    float             lastTime    = 0.0f;
    bool              seeded      = false;
    ImU32             tintRgb     = IM_COL32(232, 220, 196, 255); // aged paper
    ImU32             phosphorRgb = IM_COL32(124, 227, 139, 255);
};

// Paint the full bed — scanlines + vignette + corner motes — into the rect.
// Safe to call once per frame from any screen; the rect is normally the
// full viewport, but you can scope it to a single VkImage panel later.
void PaintBed(ImDrawList* dl, const ImVec2& min, const ImVec2& max,
              float time, State& state, bool drawScanlines = true);

// 1.0 + amp * sin(2*pi*freqHz * t + phase). Reusable for buttons + panes.
float Breath(float time, float freqHz, float amp, float phase = 0.0f);

// A subtle inner-bevel glow on a panel — "embossed glass" pass.
void PanelBevel(ImDrawList* dl, const ImVec2& min, const ImVec2& max,
                float rounding, ImU32 innerHighlight, ImU32 shadow);

// Animated 1-pixel dashed border that marches around a rect at `speed`.
void MarchingAnts(ImDrawList* dl, const ImVec2& min, const ImVec2& max,
                  float rounding, float time, float speed, ImU32 col);

// Drop a stack of falloff rectangles outside `min/max` — cheap glow.
void OuterGlow(ImDrawList* dl, const ImVec2& min, const ImVec2& max,
               float rounding, ImU32 colour, int radiusPx);

} // namespace n64xr::ui::alive
