// ============================================================================
//  BottomNav.cpp — the cyan icon nav rail.
// ----------------------------------------------------------------------------
//  GAMES -> Home; SETTINGS / CONTROLLER / VR OPTIONS -> ServiceHatch (the
//  existing settings screen hosts controller + VR options for now). Active
//  section highlighted. Drawn into the FOREGROUND draw list so it always sits
//  above the screens.
// ============================================================================

#include "BottomNav.h"
#include "AppState.h"
#include "Theme.h"
#include "Icons.h"
#include "GlassPanel.h"

#include <imgui.h>
#include <cmath>

namespace n64xr::ui {
namespace {

inline ImU32 U32(const ImVec4& c) { return ImGui::ColorConvertFloat4ToU32(c); }
inline ImU32 WithA(const ImVec4& c, float a) {
    return ImGui::ColorConvertFloat4ToU32(ImVec4(c.x, c.y, c.z, a));
}

struct NavEntry {
    const char*   label;
    icons::IconFn icon;
    Screen        target;
};

} // namespace

Screen DrawBottomNav(AppState& state, float time) {
    const auto& pal = theme::Colours();
    ImGuiViewport* vp = ImGui::GetMainViewport();

    const float barH    = 92.0f;
    const float footerH = 30.0f;
    const ImVec2 barMin(vp->WorkPos.x,
                        vp->WorkPos.y + vp->WorkSize.y - barH - footerH);
    const ImVec2 barMax(vp->WorkPos.x + vp->WorkSize.x,
                        vp->WorkPos.y + vp->WorkSize.y - footerH);

    ImDrawList* dl = ImGui::GetForegroundDrawList();

    dl->AddRectFilled(barMin, barMax, IM_COL32(6, 13, 22, 235));
    dl->AddLine(ImVec2(barMin.x, barMin.y), ImVec2(barMax.x, barMin.y),
                WithA(pal.cyan, 0.55f), 1.5f);
    dl->AddLine(ImVec2(barMin.x, barMin.y + 1), ImVec2(barMax.x, barMin.y + 1),
                WithA(pal.cyan, 0.18f), 3.0f);

    static const NavEntry kEntries[4] = {
        { "GAMES",      icons::DrawGamesGridIcon, Screen::Home },
        { "SETTINGS",   icons::DrawGearIcon,      Screen::ServiceHatch },
        { "CONTROLLER", icons::DrawGamepadIcon,   Screen::ServiceHatch },
        { "VR OPTIONS", icons::DrawVRHeadsetIcon, Screen::ServiceHatch },
    };
    constexpr int kCount = 4;

    static int s_settingsActiveIdx = 1; // default highlight = SETTINGS

    ImFont* lf = theme::GetFonts().bodySmall ? theme::GetFonts().bodySmall : ImGui::GetFont();

    const float cellW = (barMax.x - barMin.x) / (float)kCount;
    Screen result = state.currentScreen;

    for (int i = 0; i < kCount; ++i) {
        const NavEntry& e = kEntries[i];
        const ImVec2 cMin(barMin.x + i * cellW, barMin.y);
        const ImVec2 cMax(cMin.x + cellW, barMax.y);
        const ImVec2 cCenter((cMin.x + cMax.x) * 0.5f, cMin.y + barH * 0.40f);

        const bool isGames = (e.target == Screen::Home);
        const bool active  = isGames ? (state.currentScreen == Screen::Home)
                                     : (state.currentScreen == Screen::ServiceHatch &&
                                        s_settingsActiveIdx == i);

        ImGui::SetCursorScreenPos(cMin);
        ImGui::PushID(i);
        const bool pressed = ImGui::InvisibleButton("nav", ImVec2(cellW, barH));
        const bool hovered = ImGui::IsItemHovered();
        ImGui::PopID();

        if (pressed) {
            state.currentScreen = e.target;
            if (!isGames) s_settingsActiveIdx = i;
            result = e.target;
        }

        if (active) {
            dl->AddRectFilled(cMin, cMax, WithA(pal.cyan, 0.10f));
            dl->AddRectFilled(ImVec2(cMin.x + cellW * 0.18f, cMin.y),
                              ImVec2(cMax.x - cellW * 0.18f, cMin.y + 2.5f),
                              U32(pal.cyanHot));
        }

        const float pulse = active ? (0.85f + 0.15f * (0.5f + 0.5f * sinf(time * 2.0f)))
                                   : 1.0f;
        const ImU32 col = active   ? U32(pal.cyanHot)
                        : hovered  ? U32(pal.cyan)
                                   : U32(pal.coolWhiteDim);
        const float iconR = 18.0f * pulse;
        ImVec2 snapped(std::floor(cCenter.x), std::floor(cCenter.y));
        e.icon(dl, snapped, iconR, col, 2.0f);

        const float lpx = lf->LegacySize;
        const float lw  = glass::MeasureTracked(lf, lpx, e.label, 2.0f);
        const float lx  = cCenter.x - lw * 0.5f;
        const float ly  = cMin.y + barH * 0.40f + iconR + 12.0f;
        glass::TextTracked(dl, lf, lpx, ImVec2(lx, ly), col, e.label, 2.0f);

        if (active) {
            dl->AddLine(ImVec2(lx, ly + lpx + 4.0f), ImVec2(lx + lw, ly + lpx + 4.0f),
                        U32(pal.cyanHot), 1.5f);
        }
    }

    return result;
}

} // namespace n64xr::ui
