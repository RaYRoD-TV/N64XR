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

#include <imgui.h>
#include <imgui_internal.h>

#include <algorithm>
#include <cmath>
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

void DrawCard(ImDrawList* dl, ImVec2 p0, ImVec2 p1, float scale, float alpha,
              bool hero, bool transparentBody, const char* title,
              ImFont* font, const theme::Palette& pal) {
    auto A = [&](float a) { return (int)(ImClamp(a * alpha, 0.0f, 1.0f) * 255); };
    const ImU32 cyan    = U32(pal.cyan);
    const ImU32 cyanDim = U32(pal.cyanDim);
    const float r       = 12.0f * scale;

    {
        float sw = (p1.x - p0.x) * 0.55f;
        ImVec2 sc((p0.x + p1.x) * 0.5f, p1.y + 8.0f * scale);
        dl->AddRectFilled(ImVec2(sc.x - sw, sc.y), ImVec2(sc.x + sw, sc.y + 14.0f * scale),
                          IM_COL32(0, 0, 0, (int)(0.40f * 255 * alpha)), 14.0f * scale);
    }

    for (int g = 4; g >= 1; --g) {
        float e = g * 3.0f * scale;
        dl->AddRect(ImVec2(p0.x - e, p0.y - e), ImVec2(p1.x + e, p1.y + e),
                    WithA(cyan, ImClamp((0.10f / g) * alpha, 0.0f, 1.0f)),
                    r + e, 0, 2.0f);
    }

    if (!transparentBody) {
        dl->AddRectFilled(p0, p1, IM_COL32(10, 18, 30, A(0.92f)), r);
        ImVec2 s1(p1.x, ImLerp(p0.y, p1.y, 0.45f));
        dl->AddRectFilledMultiColor(p0, s1,
            IM_COL32(150, 240, 255, A(0.16f)), IM_COL32(150, 240, 255, A(0.16f)),
            IM_COL32(150, 240, 255, A(0.0f)),  IM_COL32(150, 240, 255, A(0.0f)));
    }

    dl->AddRect(p0, p1, WithA(cyan, ImClamp(alpha, 0.0f, 1.0f)), r, 0, ImMax(1.6f * scale, 1.0f));

    ImVec2 lw0(p0.x + 10 * scale, p0.y + 12 * scale);
    ImVec2 lw1(p1.x - 10 * scale, p0.y + 56 * scale);
    dl->AddRectFilled(lw0, lw1, IM_COL32(5, 11, 19, A(0.85f)), 6 * scale);
    dl->AddRect(lw0, lw1, WithA(cyanDim, ImClamp(alpha, 0.0f, 1.0f)), 6 * scale, 0, 1.0f);

    for (int i = 0; i < 3; ++i) {
        float y = ImLerp(p0.y, p1.y, 0.62f + i * 0.10f);
        dl->AddLine(ImVec2(p0.x + 14 * scale, y), ImVec2(p1.x - 14 * scale, y),
                    WithA(cyanDim, 0.5f * alpha), 1.0f);
    }

    if (title && title[0]) {
        dl->PushClipRect(lw0, lw1, true);
        const float px = (hero ? font->LegacySize * 0.95f : font->LegacySize * 0.78f);
        ImVec2 ts = font->CalcTextSizeA(px, FLT_MAX, 0.0f, title);
        ImVec2 tp((lw0.x + lw1.x) * 0.5f - ts.x * 0.5f, (lw0.y + lw1.y) * 0.5f - ts.y * 0.5f);
        dl->AddText(font, px, ImVec2(tp.x + 1, tp.y + 1), IM_COL32(0, 0, 0, A(0.7f)), title);
        dl->AddText(font, px, tp, IM_COL32(210, 250, 255, A(1.0f)), title);
        dl->PopClipRect();
    }

    if (hero) {
        ImVec2 r0(p0.x, p1.y + 4), r1(p1.x, p1.y + 4 + (p1.y - p0.y) * 0.5f);
        dl->AddRectFilledMultiColor(r0, r1,
            WithA(cyan, 0.20f * alpha), WithA(cyan, 0.20f * alpha),
            WithA(cyan, 0.0f),          WithA(cyan, 0.0f));
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
        DrawCard(dl, p0, p1, 1.0f, 0.55f, true, true, "EMPTY SLOT", font, pal);
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

    const float spread = 178.0f;
    bool activated = false;

    for (int idx : order) {
        const float d  = (float)idx - g_scroll;
        const float ad = std::fabs(d);
        const float t  = ImClamp(ad / 3.0f, 0.0f, 1.0f);
        const float scale = ImLerp(1.00f, 0.55f, t * t);
        const float alpha = ImLerp(1.00f, 0.18f, t);
        const float lift  = ImLerp(0.0f, 30.0f, t * t);
        const float dirX = (d < 0 ? -1.0f : 1.0f);
        const float x    = center.x + dirX * spread * (0.55f * ad + 0.45f * ad * ad) * 0.82f;
        const float cardW = 168.0f * scale, cardH = 232.0f * scale;
        ImVec2 p0(x - cardW * 0.5f, center.y - cardH * 0.5f + lift);
        ImVec2 p1(p0.x + cardW, p0.y + cardH);
        const bool hero = (idx == state.selectedRom);
        DrawCard(dl, p0, p1, scale, alpha, hero, hero,
                 state.roms[idx].displayName.c_str(), font, pal);
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
