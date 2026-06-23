// ============================================================================
//  HomeScreen.cpp — the lobby.  Hero button: "Step Into the Room."
// ----------------------------------------------------------------------------
//  Press the hero button → call n64xr::XrSession::initialize() and pump
//  270 frames of magenta clear, then return to the launcher.  Identical
//  behaviour bound to the "Smoke Test" menu entry.
// ============================================================================

#include "Screens.h"
#include "AppState.h"
#include "Theme.h"
#include "AliveIdle.h"

#include <imgui.h>
#include <imgui_internal.h>
#include <spdlog/spdlog.h>

#include <cmath>

// vr-scene module owns the real n64xr::XrSession (creates OpenXR instance +
// Vulkan device + per-eye swapchains, pumps magenta clears for the smoke
// test). Headset color is currently baked into XrSession::pumpFrame() — no
// per-call channel args.
#include "XrSession.h"

namespace n64xr::ui {

// ---------------------------------------------------------------------------
//  Shared widget: breathing button.
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

    const ImVec2 textSize  = ImGui::CalcTextSize(label);
    const ImVec2 size      = ImVec2(
        (textSize.x + 64.0f) * widthScale * breath,
        (textSize.y + 24.0f) * breath);

    const ImVec2 pos = ImGui::GetCursorScreenPos();
    const ImVec2 mn  = pos;
    const ImVec2 mx  = ImVec2(pos.x + size.x, pos.y + size.y);

    ImGui::InvisibleButton(label, size);
    const bool hovered = ImGui::IsItemHovered();
    const bool active  = ImGui::IsItemActive();
    const bool clicked = ImGui::IsItemClicked();

    ImDrawList* dl = ImGui::GetWindowDrawList();
    auto& pal = theme::Colours();

    // ---- outer glow ----
    const ImU32 glow = ImGui::ColorConvertFloat4ToU32(
        ImVec4(pal.brass.x, pal.brass.y, pal.brass.z,
               hovered ? 0.55f : 0.30f));
    alive::OuterGlow(dl, mn, mx, 8.0f, glow, hovered ? 14 : 8);

    // ---- fill (radial gradient: hot top, dim bottom) ----
    const ImU32 top = ImGui::ColorConvertFloat4ToU32(
        active ? pal.brassDim : (hovered ? pal.brassHot : pal.brass));
    const ImU32 bot = ImGui::ColorConvertFloat4ToU32(
        ImVec4(pal.brassDim.x * 0.6f, pal.brassDim.y * 0.6f, pal.brassDim.z * 0.6f, 1.0f));
    dl->AddRectFilledMultiColor(mn, mx, top, top, bot, bot);
    dl->AddRect(mn, mx,
                ImGui::ColorConvertFloat4ToU32(pal.brassHot), 8.0f, 0, 1.5f);

    // ---- inner bevel ----
    alive::PanelBevel(dl, mn, mx, 8.0f,
                      IM_COL32(255, 240, 200, 90),
                      IM_COL32(0, 0, 0, 120));

    // ---- marching ants on hover ----
    if (hovered) {
        alive::MarchingAnts(dl, mn, mx, 8.0f, t, 80.0f,
                            ImGui::ColorConvertFloat4ToU32(pal.brassHot));
    }

    // ---- label ----
    const ImVec2 textPos = ImVec2(
        mn.x + (size.x - textSize.x) * 0.5f,
        mn.y + (size.y - textSize.y) * 0.5f);
    dl->AddText(ImVec2(textPos.x + 1, textPos.y + 1),
                IM_COL32(0, 0, 0, 180), label);
    dl->AddText(textPos,
                ImGui::ColorConvertFloat4ToU32(pal.deepNavyBg), label);

    ImGui::PopFont();
    return clicked;
}

// ---------------------------------------------------------------------------
//  Phosphor status strip — bottom-screen "awaiting cartridge" line.
// ---------------------------------------------------------------------------
void DrawPhosphorStatusStrip(const AppState& state) {
    ImGuiViewport* vp = ImGui::GetMainViewport();
    const float stripH = 36.0f;
    const ImVec2 stripMin(vp->WorkPos.x, vp->WorkPos.y + vp->WorkSize.y - stripH);
    const ImVec2 stripMax(vp->WorkPos.x + vp->WorkSize.x, vp->WorkPos.y + vp->WorkSize.y);

    ImDrawList* dl = ImGui::GetForegroundDrawList();
    auto& pal = theme::Colours();

    // bed
    dl->AddRectFilled(stripMin, stripMax,
                      ImGui::ColorConvertFloat4ToU32(
                          ImVec4(0.025f, 0.039f, 0.071f, 1.0f)));
    // top hairline in brass
    dl->AddLine(stripMin, ImVec2(stripMax.x, stripMin.y),
                ImGui::ColorConvertFloat4ToU32(pal.brassDim), 1.0f);

    ImFont* f = theme::GetFonts().phosphor ? theme::GetFonts().phosphor : ImGui::GetFont();
    const float fontPx = f->LegacySize;  // ImGui 1.92 renamed ImFont::FontSize -> LegacySize
    const ImU32 green  = ImGui::ColorConvertFloat4ToU32(pal.phosphor);
    const ImU32 greenD = ImGui::ColorConvertFloat4ToU32(pal.phosphorDim);

    // soft phosphor glow under text
    const float gx = stripMin.x + 18.0f;
    const float gy = stripMin.y + (stripH - fontPx) * 0.5f;
    dl->AddText(f, fontPx, ImVec2(gx + 1, gy + 1), greenD, state.statusLine.c_str());
    dl->AddText(f, fontPx, ImVec2(gx,     gy),     green,  state.statusLine.c_str());

    // right-aligned XR runtime read-out
    const ImVec2 rsz = f->CalcTextSizeA(fontPx, FLT_MAX, 0.0f, state.openxrRuntime.c_str());
    const ImVec2 rp(stripMax.x - rsz.x - 18.0f, gy);
    dl->AddText(f, fontPx, rp, greenD, state.openxrRuntime.c_str());
}

// ---------------------------------------------------------------------------
//  The actual home screen.
// ---------------------------------------------------------------------------
static void RunSmokeTest(AppState& s) {
    spdlog::info("Home: 'Step Into the Room' — booting OpenXR + pumping 270 magenta frames.");
    s.statusLine = "Crossing the threshold...";
    n64xr::XrSession session;
    if (!session.initialize()) {
        spdlog::warn("XrSession::initialize() returned false — no headset?");
        s.statusLine        = "Headset did not answer. Cabinet returns to standby.";
        s.openxrRuntime     = "OpenXR runtime: unreachable.";
        s.vrSessionInFlight = false;
        return;
    }
    s.openxrRuntime = "OpenXR runtime: connected.";
    for (uint32_t i = 0; i < 270; ++i) {
        // magenta clear — the classic 'I am alive' colour.
        if (!session.pumpFrame()) break;
    }
    session.shutdown();
    s.statusLine        = "Returned from the room. 270 magenta frames delivered.";
    s.vrSessionInFlight = false;
    s.lastSmokeTestNote = "Smoke test complete.";
    spdlog::info("Smoke test complete.");
}

void DrawHomeScreen(AppState& state) {
    ImFont* disp = theme::GetFonts().display ? theme::GetFonts().display : ImGui::GetFont();
    auto& pal = theme::Colours();

    // ---- Title ----
    ImGui::PushFont(disp);
    ImGui::PushStyleColor(ImGuiCol_Text, pal.agedPaper);
    ImGui::TextUnformatted("N64XR");
    ImGui::PopStyleColor();
    ImGui::PopFont();

    ImFont* dispS = theme::GetFonts().displaySmall ? theme::GetFonts().displaySmall : ImGui::GetFont();
    ImGui::PushFont(dispS);
    ImGui::PushStyleColor(ImGuiCol_Text, pal.copper);
    ImGui::TextUnformatted("  a cabinet from a future that never happened");
    ImGui::PopStyleColor();
    ImGui::PopFont();

    ImGui::Dummy(ImVec2(0, 24));

    // ---- Three columns: lobby copy / hero button / status console --------
    const float full = ImGui::GetContentRegionAvail().x;
    const float colW = full * 0.32f;

    ImGui::BeginGroup();
    ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + colW);
    ImGui::TextWrapped(
        "You are standing in the calibration lobby. The cartridge tray is open. "
        "A faint signal whines somewhere behind the panel.  When you are ready, "
        "slot a cartridge and step into the room.");
    ImGui::PopTextWrapPos();
    ImGui::EndGroup();

    ImGui::SameLine(0, 48);

    // ---- Hero column ---------------------------------------------------
    ImGui::BeginGroup();
    ImGui::Dummy(ImVec2(0, 18));

    // centre the breathing button visually
    const float btnEstW = 360.0f;
    const float pad     = (colW - btnEstW) * 0.5f;
    if (pad > 0) ImGui::Dummy(ImVec2(pad, 0));
    ImGui::SameLine();

    if (BreathingButton("  STEP INTO THE ROOM  ", 1.0f)) {
        state.vrSessionInFlight = true;
        RunSmokeTest(state);
    }

    ImGui::Spacing();
    ImGui::Dummy(ImVec2(0, 6));
    ImGui::PushStyleColor(ImGuiCol_Text, pal.agedPaperDim);
    ImGui::TextWrapped("   ( opens the headset session and breathes for 270 frames )");
    ImGui::PopStyleColor();

    ImGui::Dummy(ImVec2(0, 14));
    if (ImGui::Button("  Slot Cartridge  "))    state.currentScreen = Screen::CartridgeVault;
    ImGui::SameLine();
    if (ImGui::Button("  Bring Out the Toolbox  ")) state.currentScreen = Screen::ServiceHatch;
    ImGui::EndGroup();

    ImGui::SameLine(0, 48);

    // ---- Right column: "console readout" -------------------------------
    ImGui::BeginGroup();
    ImFont* mono = theme::GetFonts().phosphor ? theme::GetFonts().phosphor : ImGui::GetFont();
    ImGui::PushFont(mono);
    ImGui::PushStyleColor(ImGuiCol_Text, pal.phosphor);
    ImGui::TextUnformatted("> SYSTEM");
    ImGui::Indent(12);
    ImGui::Text("power.....OK");
    ImGui::Text("phosphor..OK");
    ImGui::Text("vacuum....nominal");
    ImGui::Text("headset...%s", state.headsetStandingBy ? "standing by" : "engaged");
    if (!state.lastSmokeTestNote.empty()) {
        ImGui::Spacing();
        ImGui::PushStyleColor(ImGuiCol_Text, pal.phosphorDim);
        ImGui::TextWrapped("note: %s", state.lastSmokeTestNote.c_str());
        ImGui::PopStyleColor();
    }
    ImGui::Unindent(12);
    ImGui::PopStyleColor();
    ImGui::PopFont();
    ImGui::EndGroup();
}

} // namespace n64xr::ui
