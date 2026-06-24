// ============================================================================
//  Icons.cpp — vector icon + HUD-chrome implementations.
// ----------------------------------------------------------------------------
//  Pixel-snapped, line-style, crisp at 28-40px. Circles pass explicit segment
//  counts so small curves don't facet.
// ============================================================================

#include "Icons.h"

#include <imgui.h>
#include <imgui_internal.h>   // IM_PI, PathArcTo overloads, IM_FLOOR

#include <cmath>

namespace n64xr::ui::icons {
namespace {

inline ImVec2 P(const ImVec2& c, float r, float nx, float ny) {
    return ImVec2(c.x + nx * r, c.y + ny * r);
}
inline ImU32 WithA(ImU32 rgb, float a) {
    int ai = int(a * 255.0f);
    if (ai < 0) ai = 0; if (ai > 255) ai = 255;
    return (rgb & 0x00FFFFFFu) | (ImU32(ai) << 24);
}

} // namespace

void DrawGamesGridIcon(ImDrawList* dl, ImVec2 c, float r, ImU32 col, float thick) {
    const float gap = r * 0.12f;
    const float t   = r * 0.82f;
    const float rnd = r * 0.18f;
    struct { float sx, sy; } q[4] = {{-1,-1},{1,-1},{-1,1},{1,1}};
    for (auto& s : q) {
        ImVec2 a(c.x + (s.sx < 0 ? -t : gap), c.y + (s.sy < 0 ? -t : gap));
        ImVec2 b(c.x + (s.sx < 0 ? -gap : t), c.y + (s.sy < 0 ? -gap : t));
        dl->AddRect(a, b, col, rnd, 0, thick);
    }
}

void DrawGearIcon(ImDrawList* dl, ImVec2 c, float r, ImU32 col, float thick) {
    const int   teeth   = 8;
    const float rOuter  = r * 0.96f;
    const float rInner  = r * 0.70f;
    const float toothFr = 0.5f;
    ImVector<ImVec2> pts; pts.reserve(teeth * 4);
    for (int i = 0; i < teeth; ++i) {
        float a0 = (float)i / teeth * IM_PI * 2.0f;
        float aw = (IM_PI * 2.0f / teeth);
        float tTip      = aw * toothFr;
        float aTipStart = a0 + (aw - tTip) * 0.5f;
        float aTipEnd   = aTipStart + tTip;
        pts.push_back(ImVec2(c.x + cosf(a0)*rInner,        c.y + sinf(a0)*rInner));
        pts.push_back(ImVec2(c.x + cosf(aTipStart)*rOuter, c.y + sinf(aTipStart)*rOuter));
        pts.push_back(ImVec2(c.x + cosf(aTipEnd)*rOuter,   c.y + sinf(aTipEnd)*rOuter));
        pts.push_back(ImVec2(c.x + cosf(a0+aw)*rInner,     c.y + sinf(a0+aw)*rInner));
    }
    dl->AddPolyline(pts.Data, pts.Size, col, ImDrawFlags_Closed, thick);
    dl->AddCircle(c, r * 0.30f, col, 28, thick);
}

void DrawGamepadIcon(ImDrawList* dl, ImVec2 c, float r, ImU32 col, float thick) {
    dl->AddRect(P(c, r, -0.85f, -0.30f), P(c, r, 0.85f, 0.34f), col, r * 0.30f, 0, thick);
    ImVec2 dC = P(c, r, -0.42f, 0.02f);
    float  da = r * 0.20f, db = r * 0.06f;
    dl->AddRectFilled(ImVec2(dC.x - db, dC.y - da), ImVec2(dC.x + db, dC.y + da), col, 1.0f);
    dl->AddRectFilled(ImVec2(dC.x - da, dC.y - db), ImVec2(dC.x + da, dC.y + db), col, 1.0f);
    dl->AddCircleFilled(P(c, r, 0.40f, -0.10f), r * 0.10f, col, 14);
    dl->AddCircleFilled(P(c, r, 0.62f,  0.12f), r * 0.10f, col, 14);
}

void DrawVRHeadsetIcon(ImDrawList* dl, ImVec2 c, float r, ImU32 col, float thick) {
    dl->AddRect(P(c, r, -0.85f, -0.28f), P(c, r, 0.85f, 0.46f), col, r * 0.34f, 0, thick);
    dl->AddCircle(P(c, r, -0.40f, 0.08f), r * 0.17f, col, 18, thick * 0.85f);
    dl->AddCircle(P(c, r,  0.40f, 0.08f), r * 0.17f, col, 18, thick * 0.85f);
    dl->AddLine(P(c, r, -0.16f, 0.46f), P(c, r, 0.00f, 0.18f), col, thick);
    dl->AddLine(P(c, r,  0.00f, 0.18f), P(c, r, 0.16f, 0.46f), col, thick);
    dl->PathArcTo(c, r * 0.96f, IM_PI * 1.16f, IM_PI * 1.84f, 24);
    dl->PathStroke(col, 0, thick);
}

void DrawCartridgeIcon(ImDrawList* dl, ImVec2 c, float r, ImU32 col, float thick) {
    ImVec2 pts[8] = {
        P(c, r, -0.62f,  0.86f),
        P(c, r, -0.62f, -0.50f),
        P(c, r, -0.30f, -0.50f),
        P(c, r, -0.30f, -0.86f),
        P(c, r,  0.30f, -0.86f),
        P(c, r,  0.30f, -0.50f),
        P(c, r,  0.62f, -0.50f),
        P(c, r,  0.62f,  0.86f),
    };
    dl->AddPolyline(pts, 8, col, ImDrawFlags_Closed, thick);
    dl->AddRect(P(c, r, -0.40f, -0.34f), P(c, r, 0.40f, 0.18f), col, r * 0.08f, 0, thick * 0.85f);
}

void HoloFrame(ImDrawList* dl, ImVec2 a, ImVec2 b, float cut, ImU32 col, float thick) {
    ImVec2 p[8] = {
        {a.x + cut, a.y}, {b.x - cut, a.y}, {b.x, a.y + cut}, {b.x, b.y - cut},
        {b.x - cut, b.y}, {a.x + cut, b.y}, {a.x, b.y - cut}, {a.x, a.y + cut}
    };
    dl->AddPolyline(p, 8, col, ImDrawFlags_Closed, thick);
}

void HoloFrameGlow(ImDrawList* dl, ImVec2 a, ImVec2 b, float cut, ImU32 core,
                   int rings, float spread) {
    for (int i = rings; i >= 1; --i) {
        const float g  = i * spread;
        const float al = 0.14f * (1.0f - float(i) / float(rings + 1));
        HoloFrame(dl, ImVec2(a.x - g, a.y - g), ImVec2(b.x + g, b.y + g),
                  cut + g, WithA(core, al), 2.0f);
    }
    HoloFrame(dl, a, b, cut, WithA(core, 0.95f), 1.4f);
}

void CornerBrackets(ImDrawList* dl, ImVec2 a, ImVec2 b, float baseLen,
                    ImU32 col, float time) {
    const float pulse = 0.5f + 0.5f * sinf(time * 2.6f);
    const float len   = baseLen + baseLen * 0.4f * pulse;
    const float th    = 2.0f;
    const ImU32 c     = WithA(col, 0.62f + 0.33f * pulse);
    dl->AddLine(a, ImVec2(a.x + len, a.y), c, th);
    dl->AddLine(a, ImVec2(a.x, a.y + len), c, th);
    dl->AddLine(ImVec2(b.x, a.y), ImVec2(b.x - len, a.y), c, th);
    dl->AddLine(ImVec2(b.x, a.y), ImVec2(b.x, a.y + len), c, th);
    dl->AddLine(ImVec2(a.x, b.y), ImVec2(a.x + len, b.y), c, th);
    dl->AddLine(ImVec2(a.x, b.y), ImVec2(a.x, b.y - len), c, th);
    dl->AddLine(b, ImVec2(b.x - len, b.y), c, th);
    dl->AddLine(b, ImVec2(b.x, b.y - len), c, th);
}

void PerspectiveGrid(ImDrawList* dl, ImVec2 origin, float w, float h,
                     ImU32 col, float scrollT) {
    const ImVec2 vp(origin.x + w * 0.5f, origin.y);
    const float  baseY = origin.y + h;
    for (int i = -7; i <= 7; ++i) {
        float x = origin.x + w * 0.5f + i * (w / 14.0f);
        dl->AddLine(ImVec2(x, baseY), vp, col, 1.0f);
    }
    for (int r = 0; r < 12; ++r) {
        float raw = (float(r) + scrollT) / 12.0f;
        if (raw >= 1.0f) raw -= 1.0f;
        float t      = powf(raw, 2.2f);
        float y      = baseY - t * h;
        float spread = 1.0f - t;
        dl->AddLine(ImVec2(vp.x - w * 0.5f * spread, y),
                    ImVec2(vp.x + w * 0.5f * spread, y), col, 1.0f);
    }
}

} // namespace n64xr::ui::icons
