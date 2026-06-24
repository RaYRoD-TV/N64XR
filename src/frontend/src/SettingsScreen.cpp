// ============================================================================
//  SettingsScreen.cpp — "Service Hatch."
// ----------------------------------------------------------------------------
//  Placeholder rows for now — each is wired to AppState fields so the
//  toggles persist for the session and we can ship real persistence next.
// ============================================================================

#include "Screens.h"
#include "AppState.h"
#include "Theme.h"

#include <imgui.h>

namespace n64xr::ui {

static void Row(const char* label, const char* hint) {
    auto& pal = theme::Colours();
    ImGui::PushStyleColor(ImGuiCol_Text, pal.agedPaper);
    ImGui::TextUnformatted(label);
    ImGui::PopStyleColor();
    if (hint && *hint) {
        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Text, pal.agedPaperDim);
        ImGui::Text("   %s", hint);
        ImGui::PopStyleColor();
    }
}

void DrawSettingsScreen(AppState& state) {
    auto& pal  = theme::Colours();
    ImFont* dF = theme::GetFonts().display ? theme::GetFonts().display : ImGui::GetFont();

    ImGui::PushFont(dF);
    ImGui::PushStyleColor(ImGuiCol_Text, pal.agedPaper);
    ImGui::TextUnformatted("SERVICE HATCH");
    ImGui::PopStyleColor();
    ImGui::PopFont();

    ImGui::PushStyleColor(ImGuiCol_Text, pal.copper);
    ImGui::TextUnformatted("  pry it open. tinker. nothing is locked down.");
    ImGui::PopStyleColor();

    ImGui::Dummy(ImVec2(0, 14));

    // ---- group: lighting ----
    ImGui::PushStyleColor(ImGuiCol_ChildBg, pal.panelNavy);
    ImGui::BeginChild("##lighting", ImVec2(0, 0), ImGuiChildFlags_Borders | ImGuiChildFlags_AutoResizeY);
    Row("Cabinet Lighting", "how the room feels under your hands");
    ImGui::Dummy(ImVec2(0, 6));
    ImGui::Checkbox("  Cold Light  ( dark cabinet, brass details )", &state.coldLight);
    ImGui::SetNextItemWidth(320);
    ImGui::SliderFloat("Warm Light  ( amber bias on the scanlines )",
                       &state.warmthDial, 0.0f, 1.0f, "%.2f");
    ImGui::EndChild();
    ImGui::PopStyleColor();

    ImGui::Dummy(ImVec2(0, 10));

    // ---- group: idle ----
    ImGui::PushStyleColor(ImGuiCol_ChildBg, pal.panelNavy);
    ImGui::BeginChild("##idle", ImVec2(0, 0), ImGuiChildFlags_Borders | ImGuiChildFlags_AutoResizeY);
    Row("Idle Behaviour", "the cabinet is never fully asleep");
    ImGui::Dummy(ImVec2(0, 6));
    ImGui::Checkbox("  Alive Idle layer",                   &state.showAliveIdle);
    ImGui::Checkbox("  Scanlines",                          &state.showScanlines);
    ImGui::SetNextItemWidth(320);
    ImGui::SliderFloat("Cabinet Hum  ( ambient gain )",
                       &state.cabinetHum, 0.0f, 1.0f, "%.2f");
    ImGui::EndChild();
    ImGui::PopStyleColor();

    ImGui::Dummy(ImVec2(0, 10));

    // ---- group: cartridge vault path ----
    ImGui::PushStyleColor(ImGuiCol_ChildBg, pal.panelNavy);
    ImGui::BeginChild("##vault", ImVec2(0, 0), ImGuiChildFlags_Borders | ImGuiChildFlags_AutoResizeY);
    Row("Cartridge Shelf", "where the cabinet looks for cartridges");
    ImGui::Dummy(ImVec2(0, 6));
    char buf[512] = {};
    std::snprintf(buf, sizeof(buf), "%s", state.romScanPath.c_str());
    ImGui::SetNextItemWidth(540);
    if (ImGui::InputText("##vault_path", buf, sizeof(buf))) {
        state.romScanPath = buf;
        state.needsRescan = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("  Re-shelve now  ")) state.needsRescan = true;
    ImGui::EndChild();
    ImGui::PopStyleColor();

    ImGui::Dummy(ImVec2(0, 10));

    // ---- group: refresh ----
    ImGui::PushStyleColor(ImGuiCol_ChildBg, pal.panelNavy);
    ImGui::BeginChild("##refresh", ImVec2(0, 0), ImGuiChildFlags_Borders | ImGuiChildFlags_AutoResizeY);
    Row("Refresh Cadence", "how often the cabinet thinks per second");
    ImGui::Dummy(ImVec2(0, 6));
    ImGui::SetNextItemWidth(320);
    ImGui::SliderInt("Target heartbeat (Hz)", &state.targetFps, 30, 240);
    ImGui::PushStyleColor(ImGuiCol_Text, pal.agedPaperDim);
    ImGui::TextWrapped("   ( cosmetic for now — the swapchain is locked to FIFO/vsync. "
                       "a real frame-pacer arrives with the emulator core. )");
    ImGui::PopStyleColor();
    ImGui::EndChild();
    ImGui::PopStyleColor();
}

} // namespace n64xr::ui
