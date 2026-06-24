// ============================================================================
//  Carousel.cpp — the holographic cartridge coverflow.
// ----------------------------------------------------------------------------
//  Per-card distance-from-centre drives scale / alpha / lift / spacing. Far
//  cards drawn first, hero last (z-order = depth). The hero slot body is left
//  clear so the 3D HoloStage cartridge shows through; no ROMs -> one empty
//  placeholder card centred on the hero.
// ============================================================================

#include "Carousel.h"
#include "AppState.h"
#include "Theme.h"
#include "ArtCache.h"

#include <imgui.h>
#include <imgui_internal.h>

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

namespace n64xr::ui {
namespace {

inline ImU32 U32(const ImVec4& c) { return ImGui::ColorConvertFloat4ToU32(c); }
inline ImU32 WithA(ImU32 rgb, float a) {
    int ai = int(a * 255.0f);
    if (ai < 0) ai = 0; if (ai > 255) ai = 255;
    return (rgb & 0x00FFFFFFu) | (ImU32(ai) << 24);
}

float g_scroll = 0.0f;
bool  g_init   = false;

// Build an N64-cartridge silhouette path (generous rounded TOP corners, tight
// BOTTOM corners) into dl's current path. Convex, so it fills + strokes clean.
void CartridgePath(ImDrawList* dl, ImVec2 q0, ImVec2 q1, float rTop, float rBot) {
    rTop = ImMin(rTop, (q1.x - q0.x) * 0.5f);
    rBot = ImMin(rBot, (q1.x - q0.x) * 0.5f);
    dl->PathClear();
    dl->PathArcTo(ImVec2(q0.x + rBot, q1.y - rBot), rBot, IM_PI * 0.5f, IM_PI,        8); // bottom-left
    dl->PathArcTo(ImVec2(q0.x + rTop, q0.y + rTop), rTop, IM_PI,        IM_PI * 1.5f, 12); // top-left
    dl->PathArcTo(ImVec2(q1.x - rTop, q0.y + rTop), rTop, IM_PI * 1.5f, IM_PI * 2.0f, 12); // top-right
    dl->PathArcTo(ImVec2(q1.x - rBot, q1.y - rBot), rBot, 0.0f,         IM_PI * 0.5f,  8); // bottom-right
}

// Draw text fit to maxW, ellipsising with "..." if needed. Centred at cx.
void TextFit(ImDrawList* dl, ImFont* f, float px, float cx, float y,
             ImU32 col, const char* text, float maxW) {
    std::string s = text ? text : "";
    if (f->CalcTextSizeA(px, FLT_MAX, 0.0f, s.c_str()).x > maxW) {
        while (s.size() > 1 &&
               f->CalcTextSizeA(px, FLT_MAX, 0.0f, (s + "...").c_str()).x > maxW)
            s.pop_back();
        s += "...";
    }
    const float w = f->CalcTextSizeA(px, FLT_MAX, 0.0f, s.c_str()).x;
    dl->AddText(f, px, ImVec2(cx - w * 0.5f + 1, y + 1), (col & 0x00FFFFFFu) | 0xB0000000u, s.c_str());
    dl->AddText(f, px, ImVec2(cx - w * 0.5f,     y),     col,                                s.c_str());
}

void DrawCard(ImDrawList* dl, ImVec2 p0, ImVec2 p1, float scale, float alpha,
              bool hero, const char* title, ImTextureID art, float artAspect,
              ImFont* font, const theme::Palette& pal) {
    const float W = p1.x - p0.x, H = p1.y - p0.y;
    const float rTop = W * 0.32f, rBot = W * 0.07f;
    auto Acol = [&](ImU32 rgb, float a){ return WithA(rgb, ImClamp(a * alpha, 0.0f, 1.0f)); };
    const ImU32 cyan = U32(pal.cyan), cyanDim = U32(pal.cyanDim);

    // floor shadow
    {
        float sw = W * 0.40f;
        ImVec2 sc((p0.x + p1.x) * 0.5f, p1.y + 6.0f * scale);
        dl->AddRectFilled(ImVec2(sc.x - sw, sc.y), ImVec2(sc.x + sw, sc.y + 12.0f * scale),
                          WithA(IM_COL32(0,0,0,255), 0.35f * alpha), 8.0f * scale);
    }

    // outer glow (only the hero glows strongly — keeps the row calm)
    int glowN = hero ? 4 : 2;
    for (int g = glowN; g >= 1; --g) {
        float e = g * 2.5f * scale;
        CartridgePath(dl, ImVec2(p0.x - e, p0.y - e), ImVec2(p1.x + e, p1.y + e), rTop + e, rBot + e);
        dl->AddPolyline(dl->_Path.Data, dl->_Path.Size, Acol(cyan, 0.09f / g), ImDrawFlags_Closed, 2.0f);
        dl->PathClear();
    }

    // body (dark cartridge plastic) + cyan edge
    CartridgePath(dl, p0, p1, rTop, rBot);
    dl->AddConvexPolyFilled(dl->_Path.Data, dl->_Path.Size,
                            WithA(IM_COL32(12, 22, 34, 255), 0.97f * alpha));
    dl->AddPolyline(dl->_Path.Data, dl->_Path.Size, Acol(cyan, hero ? 1.0f : 0.7f),
                    ImDrawFlags_Closed, ImMax(1.8f * scale, 1.1f));
    dl->PathClear();

    // label / art well — large square-ish front panel
    ImVec2 lab0(p0.x + W * 0.11f, p0.y + H * 0.11f);
    ImVec2 lab1(p1.x - W * 0.11f, p0.y + H * 0.72f);
    dl->AddRectFilled(lab0, lab1, WithA(IM_COL32(5, 12, 21, 255), 0.95f * alpha), 3.0f * scale);

    if (art) {
        // fit the cover preserving aspect, centred in the well
        const float wW = lab1.x - lab0.x, wH = lab1.y - lab0.y;
        const float a  = (artAspect > 0.01f) ? artAspect : (wW / wH);
        float fitW = wW, fitH = wW / a;
        if (fitH > wH) { fitH = wH; fitW = wH * a; }
        const ImVec2 c((lab0.x + lab1.x) * 0.5f, (lab0.y + lab1.y) * 0.5f);
        dl->AddImage(art, ImVec2(c.x - fitW * 0.5f, c.y - fitH * 0.5f),
                          ImVec2(c.x + fitW * 0.5f, c.y + fitH * 0.5f),
                     ImVec2(0, 0), ImVec2(1, 1), WithA(IM_COL32(255,255,255,255), alpha));
    } else if (title && title[0]) {
        const float px = font->LegacySize * (hero ? 0.58f : 0.48f);
        const float cx = (lab0.x + lab1.x) * 0.5f;
        const float maxW = (lab1.x - lab0.x) - 8.0f * scale;
        TextFit(dl, font, px, cx, (lab0.y + lab1.y) * 0.5f - px * 0.5f,
                Acol(IM_COL32(210, 250, 255, 255), 1.0f), title, maxW);
    }
    dl->AddRect(lab0, lab1, Acol(cyanDim, 0.85f), 3.0f * scale, 0, 1.0f);

    // lower grip ridges
    for (int i = 0; i < 4; ++i) {
        float y = ImLerp(p0.y, p1.y, 0.76f + i * 0.05f);
        dl->AddLine(ImVec2(p0.x + W * 0.22f, y), ImVec2(p1.x - W * 0.22f, y),
                    Acol(cyanDim, 0.40f), 1.0f);
    }
}

} // namespace

bool DrawCarousel(AppState& state, ImVec2 a, ImVec2 b, float time) {
    (void)time;
    ImDrawList* dl  = ImGui::GetWindowDrawList();
    const auto& pal = theme::Colours();
    ImFont* font    = theme::GetFonts().displaySmall ? theme::GetFonts().displaySmall
                                                     : ImGui::GetFont();

    const int N = (int)state.roms.size();
    const ImVec2 center((a.x + b.x) * 0.5f, (a.y + b.y) * 0.55f);

    if (N == 0) {
        const float cardW = 168.0f, cardH = 232.0f;
        ImVec2 p0(center.x - cardW * 0.5f, center.y - cardH * 0.5f);
        ImVec2 p1(p0.x + cardW, p0.y + cardH);
        DrawCard(dl, p0, p1, 1.0f, 0.55f, true, "EMPTY SLOT", 0, 0.0f, font, pal);
        return false;
    }

    if (state.selectedRom < 0)  state.selectedRom = 0;
    if (state.selectedRom >= N) state.selectedRom = N - 1;
    if (!g_init) { g_scroll = (float)state.selectedRom; g_init = true; }

    if (ImGui::IsKeyPressed(ImGuiKey_RightArrow) || ImGui::IsKeyPressed(ImGuiKey_GamepadDpadRight))
        state.selectedRom = std::min(state.selectedRom + 1, N - 1);
    if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow) || ImGui::IsKeyPressed(ImGuiKey_GamepadDpadLeft))
        state.selectedRom = std::max(state.selectedRom - 1, 0);

    const float wheel = ImGui::GetIO().MouseWheel;
    if (std::fabs(wheel) > 0.0f && ImGui::IsMouseHoveringRect(a, b, false)) {
        state.selectedRom = std::clamp(state.selectedRom - (int)std::round(wheel), 0, N - 1);
    }

    const float dt = ImGui::GetIO().DeltaTime;
    const float k  = 1.0f - std::exp(-12.0f * dt);
    g_scroll += ((float)state.selectedRom - g_scroll) * k;

    std::vector<int> order(N);
    for (int i = 0; i < N; ++i) order[i] = i;
    std::sort(order.begin(), order.end(), [&](int x, int y) {
        return std::fabs(x - g_scroll) > std::fabs(y - g_scroll);
    });

    const float spread = 232.0f;
    bool activated = false;

    for (int idx : order) {
        const float d  = (float)idx - g_scroll;
        const float ad = std::fabs(d);
        const float t  = ImClamp(ad / 3.0f, 0.0f, 1.0f);
        const float scale = ImLerp(1.00f, 0.58f, t * t);
        const float alpha = ImLerp(1.00f, 0.16f, t);
        const float lift  = ImLerp(0.0f, 26.0f, t * t);
        const float dirX = (d < 0 ? -1.0f : 1.0f);
        const float x    = center.x + dirX * spread * (0.62f * ad + 0.38f * ad * ad);
        const float cardW = 164.0f * scale, cardH = 226.0f * scale;
        ImVec2 p0(x - cardW * 0.5f, center.y - cardH * 0.5f + lift);
        ImVec2 p1(p0.x + cardW, p0.y + cardH);
        const bool hero = (idx == state.selectedRom);
        int aw = 0, ah = 0;
        ImTextureID art = Art().get(state.roms[idx].displayName, &aw, &ah);
        const float aspect = (ah > 0) ? float(aw) / float(ah) : 0.0f;
        DrawCard(dl, p0, p1, scale, alpha, hero,
                 state.roms[idx].displayName.c_str(), art, aspect, font, pal);
        ImGui::SetCursorScreenPos(p0);
        ImGui::PushID(idx);
        if (ImGui::InvisibleButton("card", ImVec2(cardW, cardH))) {
            if (hero) activated = true;
            else      state.selectedRom = idx;
        }
        ImGui::PopID();
    }

    if (ImGui::IsKeyPressed(ImGuiKey_Enter) || ImGui::IsKeyPressed(ImGuiKey_GamepadFaceDown))
        activated = true;

    return activated;
}

} // namespace n64xr::ui
