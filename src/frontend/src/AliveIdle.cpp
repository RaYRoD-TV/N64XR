// ============================================================================
//  AliveIdle.cpp — scanlines, vignette, motes, breath, marching ants.
// ----------------------------------------------------------------------------
//  All work happens in ImDrawList land so this file is intentionally free
//  of Vulkan symbols.  The same code paints the launcher window AND will
//  paint into a VkImage later (Workstream G in-VR panel).
// ============================================================================

#include "AliveIdle.h"

#include <imgui.h>
#include <imgui_internal.h>

#include <algorithm>  // std::clamp
#include <cmath>
#include <cstdlib>

namespace n64xr::ui::alive {

namespace {
    constexpr float kTwoPi  = 6.28318530718f;

    inline float Rand01() {
        return static_cast<float>(std::rand()) / static_cast<float>(RAND_MAX);
    }

    inline float Vignette(float x, float y) {
        // x,y in [-1,1].  smoothstep(0.55, 1.0, length)
        const float r = std::sqrt(x*x + y*y);
        const float t = std::clamp((r - 0.55f) / 0.45f, 0.0f, 1.0f);
        return t * t * (3.0f - 2.0f * t);
    }
}

float Breath(float time, float freqHz, float amp, float phase) {
    return 1.0f + amp * std::sin(kTwoPi * freqHz * time + phase);
}

void PaintBed(ImDrawList* dl, const ImVec2& min, const ImVec2& max,
              float time, State& state, bool drawScanlines) {
    if (!dl) return;
    const float w = max.x - min.x;
    const float h = max.y - min.y;
    if (w <= 0 || h <= 0) return;

    // ---- 1.  Vignette ---------------------------------------------------
    {
        constexpr int kBands = 14;
        for (int i = 0; i < kBands; ++i) {
            const float k = static_cast<float>(i) / (kBands - 1); // 0..1
            const float inset = (1.0f - k) * std::min(w, h) * 0.5f;
            const ImVec2 mn = ImVec2(min.x + inset, min.y + inset);
            const ImVec2 mx = ImVec2(max.x - inset, max.y - inset);
            if (mx.x <= mn.x || mx.y <= mn.y) continue;
            const float alpha = (1.0f - k) * 0.06f;
            const ImU32 col = IM_COL32(0, 0, 0, static_cast<int>(alpha * 255.0f));
            dl->AddRectFilled(mn, mx, col);
        }
    }

    // ---- 2.  Scanlines (frame-locked) -----------------------------------
    if (drawScanlines) {
        const ImU32 line = IM_COL32(0, 0, 0, 26);
        const float roll = std::fmod(time * 4.0f, 2.0f);  // slow vertical roll
        for (float y = min.y + roll; y < max.y; y += 2.0f) {
            dl->AddLine(ImVec2(min.x, y), ImVec2(max.x, y), line, 1.0f);
        }
    }

    // ---- 3.  Motes ------------------------------------------------------
    if (!state.seeded) {
        state.motes.resize(110);
        for (auto& m : state.motes) {
            m.pos   = ImVec2(min.x + Rand01() * w, min.y + Rand01() * h);
            m.vel   = ImVec2((Rand01() - 0.5f) * 18.0f, (Rand01() - 0.5f) * 12.0f);
            m.phase = Rand01() * kTwoPi;
            m.size  = 0.8f + Rand01() * 1.6f;
        }
        state.seeded   = true;
        state.lastTime = time;
    }
    const float dt = std::clamp(time - state.lastTime, 0.0f, 0.05f);
    state.lastTime  = time;

    for (auto& m : state.motes) {
        // gentle curl so motes lazily orbit rather than drift
        const float cx = (min.x + max.x) * 0.5f;
        const float cy = (min.y + max.y) * 0.5f;
        const float rx = m.pos.x - cx;
        const float ry = m.pos.y - cy;
        m.vel.x += (-ry) * 0.0006f;
        m.vel.y += ( rx) * 0.0006f;
        m.pos.x += m.vel.x * dt;
        m.pos.y += m.vel.y * dt;

        // wrap
        if (m.pos.x < min.x) m.pos.x = max.x;
        if (m.pos.x > max.x) m.pos.x = min.x;
        if (m.pos.y < min.y) m.pos.y = max.y;
        if (m.pos.y > max.y) m.pos.y = min.y;

        // brightness only in the vignette ring
        const float nx = (m.pos.x - cx) / (w * 0.5f);
        const float ny = (m.pos.y - cy) / (h * 0.5f);
        const float v  = Vignette(nx, ny);
        const float pulse = 0.5f + 0.5f * std::sin(m.phase + time * 0.7f);
        const float a = v * pulse * 0.55f;
        if (a < 0.02f) continue;
        const ImU32 col = IM_COL32(232, 220, 196, static_cast<int>(a * 255.0f));
        dl->AddCircleFilled(m.pos, m.size, col, 6);
    }

    // ---- 4.  Edge phosphor sheen ---------------------------------------
    {
        const ImU32 top = IM_COL32(124, 227, 139, 14);
        const ImU32 bot = IM_COL32(124, 227, 139,  0);
        dl->AddRectFilledMultiColor(min, ImVec2(max.x, min.y + 60),
                                    top, top, bot, bot);
    }
}

void PanelBevel(ImDrawList* dl, const ImVec2& min, const ImVec2& max,
                float rounding, ImU32 innerHighlight, ImU32 shadow) {
    // inner shadow at the bottom-right
    dl->AddRect(ImVec2(min.x + 1, min.y + 1),
                ImVec2(max.x - 1, max.y - 1),
                shadow, rounding, 0, 1.0f);
    // inner highlight at the top-left
    dl->AddLine(ImVec2(min.x + rounding, min.y + 1),
                ImVec2(max.x - rounding, min.y + 1), innerHighlight, 1.0f);
    dl->AddLine(ImVec2(min.x + 1, min.y + rounding),
                ImVec2(min.x + 1, max.y - rounding), innerHighlight, 1.0f);
}

void MarchingAnts(ImDrawList* dl, const ImVec2& min, const ImVec2& max,
                  float rounding, float time, float speed, ImU32 col) {
    constexpr float kDash = 8.0f;
    constexpr float kGap  = 6.0f;
    const float period   = kDash + kGap;
    const float t        = std::fmod(time * speed, period);

    auto emit = [&](ImVec2 a, ImVec2 b) {
        const float dx = b.x - a.x;
        const float dy = b.y - a.y;
        const float len = std::sqrt(dx*dx + dy*dy);
        if (len < 1.0f) return;
        const float ux = dx / len;
        const float uy = dy / len;
        for (float p = -t; p < len; p += period) {
            const float s = std::max(p, 0.0f);
            const float e = std::min(p + kDash, len);
            if (e <= s) continue;
            dl->AddLine(ImVec2(a.x + ux * s, a.y + uy * s),
                        ImVec2(a.x + ux * e, a.y + uy * e), col, 1.5f);
        }
    };

    const float r = rounding;
    emit(ImVec2(min.x + r, min.y), ImVec2(max.x - r, min.y));
    emit(ImVec2(max.x,     min.y + r), ImVec2(max.x, max.y - r));
    emit(ImVec2(max.x - r, max.y), ImVec2(min.x + r, max.y));
    emit(ImVec2(min.x,     max.y - r), ImVec2(min.x, min.y + r));
}

void OuterGlow(ImDrawList* dl, const ImVec2& min, const ImVec2& max,
               float rounding, ImU32 colour, int radiusPx) {
    const float r = static_cast<float>(radiusPx);
    const float ca = static_cast<float>((colour >> 24) & 0xFF) / 255.0f;
    const ImU32 rgb = colour & 0x00FFFFFFu;
    for (int i = radiusPx; i > 0; --i) {
        const float k = static_cast<float>(i) / r;
        const float a = (1.0f - k) * (1.0f - k) * ca;
        const ImU32 col = rgb | (static_cast<ImU32>(a * 255.0f) << 24);
        dl->AddRect(ImVec2(min.x - i, min.y - i),
                    ImVec2(max.x + i, max.y + i),
                    col, rounding + i, 0, 1.0f);
    }
}

} // namespace n64xr::ui::alive
