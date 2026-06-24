// ============================================================================
//  HomeScreen.cpp — the holographic home. A premium cyan/teal HUD over a dark
//  navy void with a receding perspective grid. The hero is a coverflow of
//  holographic cartridge cards revolving around the 3D HoloStage cartridge
//  (rendered behind ImGui in the composite). One clean ENTER VR affordance is
//  wired to the existing XrSession smoke test.
// ============================================================================

#include "Screens.h"
#include "AppState.h"
#include "Theme.h"
#include "AliveIdle.h"
#include "GlassPanel.h"
#include "Icons.h"
#include "Carousel.h"

#include <imgui.h>
#include <imgui_internal.h>
#include <spdlog/spdlog.h>

#include <cmath>
#include <cstdio>

#include "XrSession.h"

namespace n64xr::ui {

using glass::Scrim;
using glass::TextTracked;
using glass::MeasureTracked;

namespace {

inline ImU32 U32(const ImVec4& c) { return ImGui::ColorConvertFloat4ToU32(c); }
inline ImU32 U32a(const ImVec4& c, float a) {
    return ImGui::ColorConvertFloat4ToU32(ImVec4(c.x, c.y, c.z, a));
}

} // namespace

// ---------------------------------------------------------------------------
//  Shared widget: breathing primary-action button (now cyan).
// ---------------------------------------------------------------------------
bool BreathingButton(const char* label, float widthScale) {
    ImGuiWindow* w = ImGui::GetCurrentWindow();
    if (!w || w->SkipItems) return false;

    const float t      = static_cast<float>(ImGui::GetTime());
    const float breath = alive::Breath(t, 0.5f, 0.015f);

    ImFont* useFont = theme::GetFonts().displaySmall
                        ? theme::GetFonts().displaySmall
                        : ImGui::GetFont();
    ImGui::PushFont(useFont);

    const ImVec2 textSize = ImGui::CalcTextSize(label);
    const ImVec2 size = ImVec2((textSize.x + 80.0f) * widthScale * breath,
                               (textSize.y + 32.0f) * breath);

    const ImVec2 pos = ImGui::GetCursorScreenPos();
    const ImVec2 mn  = pos;
    const ImVec2 mx  = ImVec2(pos.x + size.x, pos.y + size.y);

    ImGui::InvisibleButton(label, size);
    const bool hovered = ImGui::IsItemHovered();
    const bool active  = ImGui::IsItemActive();
    const bool clicked = ImGui::IsItemClicked();

    ImDrawList* dl = ImGui::GetWindowDrawList();
    auto& pal = theme::Colours();

    const ImU32 glow = U32a(pal.cyan, hovered ? 0.55f : 0.32f);
    alive::OuterGlow(dl, mn, mx, 4.0f, glow, hovered ? 16 : 10);

    const ImU32 top = U32a(active ? pal.cyanDim : (hovered ? pal.cyanHot : pal.cyan), 0.22f);
    const ImU32 bot = U32a(pal.cyanDim, 0.30f);
    dl->AddRectFilledMultiColor(mn, mx, top, top, bot, bot);
    dl->AddRect(mn, mx, U32(hovered ? pal.cyanHot : pal.cyan), 4.0f, 0, 1.6f);

    if (hovered)
        alive::MarchingAnts(dl, mn, mx, 4.0f, t, 90.0f, U32(pal.cyanHot));

    const ImVec2 textPos = ImVec2(mn.x + (size.x - textSize.x) * 0.5f,
                                  mn.y + (size.y - textSize.y) * 0.5f);
    dl->AddText(ImVec2(textPos.x + 1, textPos.y + 1), IM_COL32(0, 0, 0, 180), label);
    dl->AddText(textPos, U32(pal.coolWhite), label);

    ImGui::PopFont();
    return clicked;
}

// ---------------------------------------------------------------------------
//  Footer status strip — restyled cyan/green, crisp + plain.
// ---------------------------------------------------------------------------
void DrawPhosphorStatusStrip(const AppState& state) {
    ImGuiViewport* vp = ImGui::GetMainViewport();
    const float stripH = 30.0f;
    const ImVec2 stripMin(vp->WorkPos.x, vp->WorkPos.y + vp->WorkSize.y - stripH);
    const ImVec2 stripMax(vp->WorkPos.x + vp->WorkSize.x, vp->WorkPos.y + vp->WorkSize.y);

    ImDrawList* dl = ImGui::GetForegroundDrawList();
    auto& pal = theme::Colours();

    dl->AddRectFilled(stripMin, stripMax, IM_COL32(4, 9, 16, 240));
    dl->AddLine(stripMin, ImVec2(stripMax.x, stripMin.y), U32a(pal.cyanDim, 0.8f), 1.0f);

    ImFont* f = theme::GetFonts().bodySmall ? theme::GetFonts().bodySmall : ImGui::GetFont();
    const float fontPx = f->LegacySize;

    const float gx = stripMin.x + 18.0f;
    const float gy = stripMin.y + (stripH - fontPx) * 0.5f;
    dl->AddText(f, fontPx, ImVec2(gx, gy), U32(pal.activeGreen), state.statusLine.c_str());

    const ImVec2 rsz = f->CalcTextSizeA(fontPx, FLT_MAX, 0.0f, state.openxrRuntime.c_str());
    const ImVec2 rp(stripMax.x - rsz.x - 18.0f, gy);
    dl->AddText(f, fontPx, rp, U32(pal.coolWhiteDim), state.openxrRuntime.c_str());
}

// ---------------------------------------------------------------------------
//  Smoke test (behaviour unchanged — keeps the XrSession call wired).
// ---------------------------------------------------------------------------
static void RunSmokeTest(AppState& s) {
    spdlog::info("Home: ENTER VR — booting OpenXR + 270 magenta frames.");
    s.statusLine = "Entering VR...";
    n64xr::XrSession session;
    if (!session.initialize()) {
        spdlog::warn("XrSession::initialize() returned false — OpenXR init failed "
                     "(no active VR session? connect Virtual Desktop / SteamVR / Oculus).");
        s.statusLine    = "VR session unavailable — connect your headset "
                          "(start Virtual Desktop / SteamVR), then press ENTER VR.";
        s.openxrRuntime = "OpenXR: init failed — no active VR session.";
        s.vrSessionInFlight = false;
        return;
    }
    s.openxrRuntime = "OpenXR runtime: connected.";
    for (uint32_t i = 0; i < 270; ++i) {
        if (!session.pumpFrame()) break;
    }
    session.shutdown();
    s.statusLine        = "Returned from VR. 270 frames delivered.";
    s.vrSessionInFlight = false;
    s.lastSmokeTestNote = "VR session OK.";
    spdlog::info("Smoke test complete.");
}

// ---------------------------------------------------------------------------
//  The holographic home screen.
// ---------------------------------------------------------------------------
void DrawHomeScreen(AppState& state) {
    auto& pal = theme::Colours();
    const auto& fonts = theme::GetFonts();
    ImGuiViewport* vp = ImGui::GetMainViewport();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const float t = static_cast<float>(ImGui::GetTime());

    const ImVec2 origin = ImGui::GetCursorScreenPos();
    const float  W = ImGui::GetContentRegionAvail().x;
    const float  navReserve = 92.0f + 30.0f + 8.0f;
    const float  H = vp->WorkPos.y + vp->WorkSize.y - origin.y - navReserve;

    const float marginX = W * 0.04f;
    const float left    = origin.x + marginX;
    const float right   = origin.x + W - marginX;
    const float top     = origin.y + 14.0f;
    const float bottom  = origin.y + H;

    // The room interior (floor / walls / back, perspective grid) is now drawn
    // by the composite shader behind ImGui — no ImGui-drawn grid here (it
    // conflicted as stray diagonals over the shader room).

    // ===================== TOP: title lockup + VR badge =====================
    ImFont* disp  = fonts.display ? fonts.display : ImGui::GetFont();
    ImFont* dispS = fonts.displaySmall ? fonts.displaySmall : ImGui::GetFont();

    const float titlePx = disp->LegacySize * 1.55f;
    TextTracked(dl, disp, titlePx, ImVec2(left, top),
                U32a(pal.cyan, 0.30f), "N64 XR", titlePx * 0.12f);
    TextTracked(dl, disp, titlePx, ImVec2(left, top),
                U32(pal.coolWhite), "N64 XR", titlePx * 0.12f);

    {
        ImFont* bf = fonts.bodySmall ? fonts.bodySmall : ImGui::GetFont();
        const char* badge = "VR MODE: ACTIVE";
        const float bpx = bf->LegacySize;
        const float bw  = MeasureTracked(bf, bpx, badge, 2.0f);
        const float padX = 16.0f, padY = 8.0f, dot = 5.0f;
        const float bx1 = right;
        const float bx0 = bx1 - bw - padX * 2.0f - dot * 2.0f - 8.0f;
        const float by0 = top + 4.0f;
        const float by1 = by0 + bpx + padY * 2.0f;
        ImVec2 a(bx0, by0), b(bx1, by1);
        dl->AddRectFilled(a, b, IM_COL32(6, 22, 16, 220), (by1 - by0) * 0.5f);
        dl->AddRect(a, b, U32a(pal.activeGreen, 0.7f), (by1 - by0) * 0.5f, 0, 1.2f);
        const float dpulse = 0.6f + 0.4f * (0.5f + 0.5f * sinf(t * 2.4f));
        ImVec2 dc(bx0 + padX + dot, (by0 + by1) * 0.5f);
        dl->AddCircleFilled(dc, dot, U32a(pal.activeGreen, dpulse), 16);
        glass::TextTracked(dl, bf, bpx, ImVec2(dc.x + dot + 8.0f, by0 + padY),
                           U32(pal.activeGreen), badge, 2.0f);
    }

    // ===================== HERO: framed coverflow =====================
    const float heroTop    = top + titlePx + 36.0f;
    const float metaH      = 120.0f;
    const float heroBottom = bottom - metaH;
    const float heroW      = (right - left) * 0.92f;
    const ImVec2 heroA(left + (right - left - heroW) * 0.5f, heroTop);
    const ImVec2 heroB(heroA.x + heroW, heroBottom);

    icons::HoloFrameGlow(dl, heroA, heroB, 26.0f, U32(pal.cyan), 4, 2.2f);
    icons::CornerBrackets(dl, heroA, heroB, 18.0f, U32(pal.cyanHot), t);

    dl->AddRectFilledMultiColor(heroA, ImVec2(heroB.x, heroA.y + 60.0f),
        IM_COL32(8, 16, 28, 70), IM_COL32(8, 16, 28, 70),
        IM_COL32(8, 16, 28, 0),  IM_COL32(8, 16, 28, 0));

    const bool activated = DrawCarousel(state, heroA, heroB, t);

    // ===================== UNDER HERO: selected name + meta =====================
    {
        const char* name = (!state.roms.empty() && state.selectedRom >= 0 &&
                            state.selectedRom < (int)state.roms.size())
                               ? state.roms[state.selectedRom].displayName.c_str()
                               : "No cartridges found";

        const float nmPx = dispS->LegacySize * 1.05f;
        const float nmW  = MeasureTracked(dispS, nmPx, name, nmPx * 0.04f);
        const float cx   = (heroA.x + heroB.x) * 0.5f;
        const float ny   = heroBottom + 8.0f;
        ImVec2 sa(cx - nmW * 0.5f - 24.0f, ny - 6.0f);
        ImVec2 sb(cx + nmW * 0.5f + 24.0f, ny + nmPx + 8.0f);
        Scrim(dl, sa, sb, IM_COL32(4, 9, 16, 150));
        TextTracked(dl, dispS, nmPx, ImVec2(cx - nmW * 0.5f, ny),
                    U32(pal.coolWhite), name, nmPx * 0.04f);

        ImFont* bf = fonts.bodySmall ? fonts.bodySmall : ImGui::GetFont();
        char meta[160];
        if (!state.roms.empty()) {
            std::snprintf(meta, sizeof(meta), "Nintendo 64  -  %s  -  %d of %d",
                          (state.selectedRom >= 0 &&
                           state.selectedRom < (int)state.roms.size() &&
                           !state.roms[state.selectedRom].extension.empty())
                              ? state.roms[state.selectedRom].extension.c_str()
                              : "rom",
                          state.selectedRom + 1, (int)state.roms.size());
        } else {
            std::snprintf(meta, sizeof(meta), "Scan path: %s", state.romScanPath.c_str());
        }
        const float mpx = bf->LegacySize;
        const float mw  = MeasureTracked(bf, mpx, meta, 1.5f);
        TextTracked(dl, bf, mpx, ImVec2(cx - mw * 0.5f, ny + nmPx + 8.0f),
                    U32(pal.coolWhiteDim), meta, 1.5f);
    }

    // ===================== PRIMARY: ENTER VR =====================
    {
        const float cx = (heroA.x + heroB.x) * 0.5f;
        ImGui::PushFont(dispS);
        const ImVec2 btnText = ImGui::CalcTextSize("  ENTER VR  ");
        ImGui::PopFont();
        const float btnW = (btnText.x + 80.0f);
        const float by   = bottom - 44.0f;
        ImGui::SetCursorScreenPos(ImVec2(cx - btnW * 0.5f, by));
        if (BreathingButton("  ENTER VR  ", 1.0f) || activated) {
            state.vrSessionInFlight = true;
            RunSmokeTest(state);
        }
    }

    ImGui::SetCursorScreenPos(ImVec2(origin.x, bottom + 2.0f));
    ImGui::Dummy(ImVec2(1, 1));
}

} // namespace n64xr::ui
