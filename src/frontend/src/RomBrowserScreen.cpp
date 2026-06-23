// ============================================================================
//  RomBrowserScreen.cpp — "Cartridge Vault."
// ----------------------------------------------------------------------------
//  Walks AppState::romScanPath with std::filesystem; lists .n64/.v64/.z64.
//  No ROM loading yet — that comes online when the emulator core is wired
//  (Phase 1b).  This screen just proves the data path + visual identity.
// ============================================================================

#include "Screens.h"
#include "AppState.h"
#include "Theme.h"

#include <imgui.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <string>

namespace n64xr::ui {

namespace fs = std::filesystem;

static bool IsCartridgeFile(const fs::path& p) {
    if (!p.has_extension()) return false;
    std::string ext = p.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(),
                   [](unsigned char c){ return static_cast<char>(std::tolower(c)); });
    return ext == ".n64" || ext == ".v64" || ext == ".z64";
}

static void Rescan(AppState& s) {
    s.roms.clear();
    fs::path root(s.romScanPath);
    if (!fs::exists(root) || !fs::is_directory(root)) {
        s.statusLine = "Cartridge vault is empty — check the path under Service Hatch.";
        return;
    }
    std::error_code ec;
    for (auto& entry : fs::recursive_directory_iterator(root,
                            fs::directory_options::skip_permission_denied, ec)) {
        if (ec) { ec.clear(); continue; }
        if (!entry.is_regular_file(ec)) continue;
        if (!IsCartridgeFile(entry.path())) continue;

        RomEntry r;
        r.path        = entry.path();
        r.displayName = entry.path().stem().string();
        r.extension   = entry.path().extension().string();
        r.sizeBytes   = static_cast<std::uintmax_t>(
                            entry.file_size(ec));
        if (ec) { r.sizeBytes = 0; ec.clear(); }
        s.roms.push_back(std::move(r));
    }
    std::sort(s.roms.begin(), s.roms.end(),
              [](const RomEntry& a, const RomEntry& b) {
                  return a.displayName < b.displayName;
              });
    s.statusLine = std::string("Vault holds ") + std::to_string(s.roms.size())
                 + " cartridge" + (s.roms.size() == 1 ? "" : "s") + ".";
    spdlog::info("Cartridge Vault scan: {} files under '{}'.",
                 s.roms.size(), s.romScanPath);
}

static std::string PrettySize(std::uintmax_t b) {
    constexpr double KB = 1024.0;
    constexpr double MB = KB * 1024.0;
    char buf[64];
    if (b > static_cast<std::uintmax_t>(MB))
        std::snprintf(buf, sizeof(buf), "%.1f MiB", b / MB);
    else if (b > static_cast<std::uintmax_t>(KB))
        std::snprintf(buf, sizeof(buf), "%.1f KiB", b / KB);
    else
        std::snprintf(buf, sizeof(buf), "%llu bytes", (unsigned long long)b);
    return buf;
}

void DrawRomBrowserScreen(AppState& state) {
    if (state.needsRescan) { Rescan(state); state.needsRescan = false; }

    auto& pal  = theme::Colours();
    ImFont* dF = theme::GetFonts().display ? theme::GetFonts().display : ImGui::GetFont();

    ImGui::PushFont(dF);
    ImGui::PushStyleColor(ImGuiCol_Text, pal.agedPaper);
    ImGui::TextUnformatted("CARTRIDGE VAULT");
    ImGui::PopStyleColor();
    ImGui::PopFont();

    ImGui::PushStyleColor(ImGuiCol_Text, pal.copper);
    ImGui::TextUnformatted("  the shelf of plastic miracles");
    ImGui::PopStyleColor();

    ImGui::Dummy(ImVec2(0, 14));

    // ---- path + actions ----
    ImGui::PushStyleColor(ImGuiCol_Text, pal.agedPaperDim);
    ImGui::Text("Scanning:");
    ImGui::PopStyleColor();
    ImGui::SameLine();

    char buf[512] = {};
    std::snprintf(buf, sizeof(buf), "%s", state.romScanPath.c_str());
    ImGui::SetNextItemWidth(540.0f);
    if (ImGui::InputText("##scanpath", buf, sizeof(buf))) {
        state.romScanPath = buf;
    }
    ImGui::SameLine();
    if (ImGui::Button("  Re-shelve  ")) state.needsRescan = true;
    ImGui::SameLine();
    if (ImGui::Button("  Empty Hands  ")) {
        state.selectedRom = -1;
        state.statusLine  = "No cartridge held.";
    }

    ImGui::Dummy(ImVec2(0, 12));

    // ---- table ----
    constexpr ImGuiTableFlags flags =
        ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerH |
        ImGuiTableFlags_ScrollY | ImGuiTableFlags_SizingStretchProp;

    if (ImGui::BeginTable("##roms", 3, flags, ImVec2(0, -64))) {
        ImGui::TableSetupColumn("Cartridge", ImGuiTableColumnFlags_WidthStretch, 0.55f);
        ImGui::TableSetupColumn("Format",    ImGuiTableColumnFlags_WidthFixed,   90.0f);
        ImGui::TableSetupColumn("Weight",    ImGuiTableColumnFlags_WidthFixed,  130.0f);
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableHeadersRow();

        for (int i = 0; i < (int)state.roms.size(); ++i) {
            const auto& r = state.roms[i];
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            const bool selected = (i == state.selectedRom);
            if (ImGui::Selectable(r.displayName.c_str(), selected,
                                  ImGuiSelectableFlags_SpanAllColumns)) {
                state.selectedRom = i;
                state.statusLine  = "Holding \"" + r.displayName + "\". Slot when ready.";
            }
            ImGui::TableSetColumnIndex(1);
            ImGui::TextUnformatted(r.extension.c_str());
            ImGui::TableSetColumnIndex(2);
            ImGui::TextUnformatted(PrettySize(r.sizeBytes).c_str());
        }

        if (state.roms.empty()) {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::PushStyleColor(ImGuiCol_Text, pal.agedPaperDim);
            ImGui::TextUnformatted("  ... empty shelf. Point Scanning at a folder of .n64/.v64/.z64.");
            ImGui::PopStyleColor();
        }
        ImGui::EndTable();
    }

    // ---- footer ----
    ImGui::Dummy(ImVec2(0, 8));
    if (state.selectedRom >= 0 && state.selectedRom < (int)state.roms.size()) {
        const auto& r = state.roms[state.selectedRom];
        ImGui::PushStyleColor(ImGuiCol_Text, pal.brassHot);
        ImGui::Text("Held: %s", r.displayName.c_str());
        ImGui::PopStyleColor();
        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Text, pal.agedPaperDim);
        ImGui::Text("  ( %s )", r.path.string().c_str());
        ImGui::PopStyleColor();
    } else {
        ImGui::PushStyleColor(ImGuiCol_Text, pal.agedPaperDim);
        ImGui::TextUnformatted("No cartridge held.");
        ImGui::PopStyleColor();
    }
}

} // namespace n64xr::ui
