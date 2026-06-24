// ============================================================================
//  GlassPanel.cpp — frosted instrument chrome.
// ----------------------------------------------------------------------------
//  All cheap AddRect / AddText calls. A static jitter table gives stable frost
//  grain (animated grain looks like TV static, not glass).
// ============================================================================

#include "GlassPanel.h"

#include <cstring>
#include <vector>

namespace n64xr::ui::glass {
namespace {

// Stable pseudo-random grain offsets in [0,1)^2, generated once.
const std::vector<ImVec2>& GrainTable() {
    static std::vector<ImVec2> table = [] {
        std::vector<ImVec2> t;
        t.reserve(360);
        uint32_t s = 0x1234567u;
        auto rnd = [&] {
            s ^= s << 13; s ^= s >> 17; s ^= s << 5;
            return float(s & 0xFFFFFFu) / float(0x1000000u);
        };
        for (int i = 0; i < 360; ++i) t.push_back(ImVec2(rnd(), rnd()));
        return t;
    }();
    return table;
}

inline ImU32 withAlpha(ImU32 rgb, float a) {
    int ai = int(a * 255.0f);
    if (ai < 0) ai = 0; if (ai > 255) ai = 255;
    return (rgb & 0x00FFFFFFu) | (ImU32(ai) << 24);
}

} // namespace

void EdgeGlow(ImDrawList* dl, ImVec2 a, ImVec2 b, float rounding,
              ImU32 accent, int rings, float spread) {
    for (int i = rings; i >= 1; --i) {
        const float g  = i * spread;
        const float al = 0.16f * (1.0f - float(i) / float(rings));
        dl->AddRect(ImVec2(a.x - g, a.y - g), ImVec2(b.x + g, b.y + g),
                    withAlpha(accent, al), rounding + g, 0, spread);
    }
    dl->AddRect(a, b, withAlpha(accent, 0.95f), rounding, 0, 1.4f); // hot core line
}

void Panel(ImDrawList* dl, ImVec2 a, ImVec2 b, float rounding,
           ImU32 tint, ImU32 accent, float accentStrength) {
    // ---- soft drop shadow (downward-biased) ----
    for (int i = 6; i >= 1; --i) {
        const float t   = float(i) / 6.0f;
        const float gsp = 10.0f * t;
        const float al  = (1.0f - t) * 0.11f;
        dl->AddRectFilled(ImVec2(a.x - gsp, a.y - gsp + 5.0f),
                          ImVec2(b.x + gsp, b.y + gsp + 8.0f),
                          withAlpha(IM_COL32(0, 0, 0, 255), al), rounding + gsp);
    }

    dl->PushClipRect(a, b, true);

    // ---- frosted body fill ----
    dl->AddRectFilled(a, b, tint, rounding);

    // ---- vertical sheen (catches 'sky' light at the top) ----
    const ImU32 sheenTop = IM_COL32(120, 150, 175, 46);
    const ImU32 sheenBot = IM_COL32(8, 12, 20, 24);
    dl->AddRectFilledMultiColor(a, b, sheenTop, sheenTop, sheenBot, sheenBot);

    // ---- top wet highlight band (~32% height) ----
    const float hh = (b.y - a.y) * 0.32f;
    dl->AddRectFilledMultiColor(a, ImVec2(b.x, a.y + hh),
                                IM_COL32(255, 255, 255, 38), IM_COL32(255, 255, 255, 38),
                                IM_COL32(255, 255, 255, 0),  IM_COL32(255, 255, 255, 0));

    // ---- frost grain ----
    const auto& grain = GrainTable();
    const float w = b.x - a.x, h = b.y - a.y;
    for (const auto& g : grain) {
        const ImVec2 p(a.x + g.x * w, a.y + g.y * h);
        dl->AddRectFilled(p, ImVec2(p.x + 1.0f, p.y + 1.0f), IM_COL32(255, 255, 255, 9));
    }

    dl->PopClipRect();

    // ---- inner sheen + crisp edge (glass thickness) ----
    dl->AddRect(ImVec2(a.x + 1, a.y + 1), ImVec2(b.x - 1, b.y - 1),
                IM_COL32(255, 255, 255, 50), rounding, 0, 1.0f);
    dl->AddRect(a, b, withAlpha(accent, 0.55f), rounding, 0, 1.2f);

    // ---- outer accent glow ----
    EdgeGlow(dl, a, b, rounding, accent, int(3 + 2 * accentStrength), 1.6f);
}

void Scrim(ImDrawList* dl, ImVec2 a, ImVec2 b, ImU32 dark) {
    const float midY = (a.y + b.y) * 0.5f;
    dl->AddRectFilledMultiColor(a, ImVec2(b.x, midY), dark, dark, dark, dark);
    const ImU32 clear = dark & 0x00FFFFFFu;
    dl->AddRectFilledMultiColor(ImVec2(a.x, midY), b, dark, dark, clear, clear);
}

float MeasureTracked(ImFont* font, float px, const char* text, float tracking) {
    float x = 0.0f;
    for (const char* c = text; *c; ++c) {
        char tmp[2] = { *c, 0 };
        x += font->CalcTextSizeA(px, FLT_MAX, 0.0f, tmp).x + tracking;
    }
    return x;
}

float TextTracked(ImDrawList* dl, ImFont* font, float px, ImVec2 pos,
                  ImU32 col, const char* text, float tracking) {
    ImVec2 p = pos;
    for (const char* c = text; *c; ++c) {
        char tmp[2] = { *c, 0 };
        // 1px shadow for legibility over the moving 3D area.
        dl->AddText(font, px, ImVec2(p.x + 1.0f, p.y + 1.0f), IM_COL32(0, 0, 0, 170), tmp);
        dl->AddText(font, px, p, col, tmp);
        p.x += font->CalcTextSizeA(px, FLT_MAX, 0.0f, tmp).x + tracking;
    }
    return p.x - pos.x;
}

} // namespace n64xr::ui::glass
