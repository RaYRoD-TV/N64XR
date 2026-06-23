// ============================================================================
//  Theme.cpp — palette + typography + spacing.
// ----------------------------------------------------------------------------
//  No external state besides ImGui's font atlas + style.  Fonts are looked
//  up under <exe-dir>/assets/fonts/; if any are missing we log a warning
//  and fall back to the built-in proggy so the launcher still boots.
// ============================================================================

#include "Theme.h"

#include <imgui.h>
#include <spdlog/spdlog.h>

#include <cmath>      // std::floor
#include <filesystem>
#include <string>

namespace n64xr::ui::theme {

namespace {
    Palette g_palette{};
    Fonts   g_fonts{};

    std::filesystem::path AssetsRoot() {
        // We expect: <cwd>/assets/fonts/  (UiHost CMake copies them in)
        std::filesystem::path p = std::filesystem::current_path() / "assets" / "fonts";
        return p;
    }

    ImFont* TryLoadFont(ImFontAtlas* atlas, const char* file, float px) {
        const auto full = AssetsRoot() / file;
        if (!std::filesystem::exists(full)) {
            spdlog::warn("Theme: missing font '{}' — falling back to default.",
                         full.string());
            return nullptr;
        }
        ImFontConfig cfg{};
        cfg.SizePixels        = px;
        cfg.RasterizerDensity = 1.0f;  // we already pre-scaled px by DPI
        cfg.OversampleH       = 2;
        cfg.OversampleV       = 1;
        cfg.PixelSnapH        = true;
        return atlas->AddFontFromFileTTF(full.string().c_str(), px, &cfg);
    }
} // namespace

const Palette& Colours()  { return g_palette; }
const Fonts&   GetFonts() { return g_fonts;   }

// ---------------------------------------------------------------------------
//  Font loading — call BEFORE ImGui_ImplVulkan_Init().
// ---------------------------------------------------------------------------
void LoadFonts(float dpiScale) {
    ImGuiIO& io = ImGui::GetIO();
    ImFontAtlas* atlas = io.Fonts;
    atlas->Clear();

    // Always add a default first so something renders if every TTF is missing.
    atlas->AddFontDefault();

    auto px = [&](float pt) { return std::floor(pt * dpiScale); };

    g_fonts.body         = TryLoadFont(atlas, "JetBrainsMono-Regular.ttf", px(16.0f));
    g_fonts.bodySmall    = TryLoadFont(atlas, "JetBrainsMono-Regular.ttf", px(13.0f));
    g_fonts.display      = TryLoadFont(atlas, "Orbitron-Bold.ttf",         px(36.0f));
    g_fonts.displaySmall = TryLoadFont(atlas, "Orbitron-Regular.ttf",      px(20.0f));
    g_fonts.phosphor     = TryLoadFont(atlas, "VT323-Regular.ttf",         px(22.0f));

    // If JetBrainsMono is missing, promote default to body so calls to
    // PushFont(body) don't crash with nullptr.
    if (!g_fonts.body) {
        spdlog::warn("Theme: JetBrainsMono missing — using ImGui default as body face.");
        g_fonts.body = atlas->Fonts.back();
    }
    // ImGui 1.92 bakes font textures lazily on first use — the old
    // ImFontAtlas::Build() is no longer needed (and was removed).
}

// ---------------------------------------------------------------------------
//  Style — palette + spacing + rounding.
// ---------------------------------------------------------------------------
void ApplyStyle(float dpiScale) {
    ImGuiStyle& s = ImGui::GetStyle();
    ImVec4* c     = s.Colors;
    const Palette& p = g_palette;

    // Convenience: a darker variant of brass for "active" state.
    auto mul = [](ImVec4 col, float k) {
        return ImVec4(col.x * k, col.y * k, col.z * k, col.w);
    };
    auto with_alpha = [](ImVec4 col, float a) {
        return ImVec4(col.x, col.y, col.z, a);
    };

    c[ImGuiCol_Text]                  = p.agedPaper;
    c[ImGuiCol_TextDisabled]          = p.agedPaperDim;
    c[ImGuiCol_TextSelectedBg]        = with_alpha(p.brass, 0.45f);

    c[ImGuiCol_WindowBg]              = p.deepNavyBg;
    c[ImGuiCol_ChildBg]               = p.panelNavy;
    c[ImGuiCol_PopupBg]               = p.panelNavyRaised;
    c[ImGuiCol_MenuBarBg]             = p.panelNavyRaised;

    c[ImGuiCol_Border]                = p.borderWarm;
    c[ImGuiCol_BorderShadow]          = ImVec4(0,0,0,0);

    c[ImGuiCol_FrameBg]               = p.panelNavyRaised;
    c[ImGuiCol_FrameBgHovered]        = with_alpha(p.brass, 0.20f);
    c[ImGuiCol_FrameBgActive]         = with_alpha(p.brass, 0.40f);

    c[ImGuiCol_TitleBg]               = p.panelNavy;
    c[ImGuiCol_TitleBgActive]         = p.panelNavyRaised;
    c[ImGuiCol_TitleBgCollapsed]      = p.panelNavy;

    c[ImGuiCol_Button]                = with_alpha(p.brass,    0.16f);
    c[ImGuiCol_ButtonHovered]         = with_alpha(p.brassHot, 0.55f);
    c[ImGuiCol_ButtonActive]          = mul(p.brassHot, 0.80f);

    c[ImGuiCol_Header]                = with_alpha(p.brass, 0.20f);
    c[ImGuiCol_HeaderHovered]         = with_alpha(p.brass, 0.45f);
    c[ImGuiCol_HeaderActive]          = with_alpha(p.brassHot, 0.65f);

    c[ImGuiCol_CheckMark]             = p.brassHot;
    c[ImGuiCol_SliderGrab]            = p.brass;
    c[ImGuiCol_SliderGrabActive]      = p.brassHot;

    c[ImGuiCol_Tab]                   = p.panelNavyRaised;
    c[ImGuiCol_TabHovered]            = with_alpha(p.brass, 0.55f);
    c[ImGuiCol_TabActive]             = with_alpha(p.brassHot, 0.50f);
    c[ImGuiCol_TabUnfocused]          = p.panelNavy;
    c[ImGuiCol_TabUnfocusedActive]    = with_alpha(p.brass, 0.30f);

    c[ImGuiCol_Separator]             = with_alpha(p.brassDim, 0.45f);
    c[ImGuiCol_SeparatorHovered]      = p.brass;
    c[ImGuiCol_SeparatorActive]       = p.brassHot;

    c[ImGuiCol_ScrollbarBg]           = with_alpha(p.deepNavyBg, 0.0f);
    c[ImGuiCol_ScrollbarGrab]         = with_alpha(p.brassDim, 0.65f);
    c[ImGuiCol_ScrollbarGrabHovered]  = p.brass;
    c[ImGuiCol_ScrollbarGrabActive]   = p.brassHot;

    c[ImGuiCol_ResizeGrip]            = with_alpha(p.brassDim, 0.30f);
    c[ImGuiCol_ResizeGripHovered]     = p.brass;
    c[ImGuiCol_ResizeGripActive]      = p.brassHot;

    c[ImGuiCol_NavHighlight]          = p.brassHot;
    c[ImGuiCol_NavWindowingHighlight] = with_alpha(p.brassHot, 0.85f);
    c[ImGuiCol_NavWindowingDimBg]     = ImVec4(0,0,0,0.40f);
    c[ImGuiCol_ModalWindowDimBg]      = ImVec4(0,0,0,0.55f);

    c[ImGuiCol_PlotLines]             = p.brass;
    c[ImGuiCol_PlotLinesHovered]      = p.brassHot;
    c[ImGuiCol_PlotHistogram]         = p.copper;
    c[ImGuiCol_PlotHistogramHovered]  = p.brassHot;

    // ---- Geometry — instruments-panel-y, never balloon-y ------------------
    s.WindowPadding       = ImVec2(18, 16);
    s.FramePadding        = ImVec2(12,  7);
    s.CellPadding         = ImVec2( 8,  4);
    s.ItemSpacing         = ImVec2(10,  8);
    s.ItemInnerSpacing    = ImVec2( 6,  4);
    s.IndentSpacing       = 22.0f;
    s.ScrollbarSize       = 14.0f;
    s.GrabMinSize         = 12.0f;

    s.WindowRounding      = 0.0f;  // launcher fills the screen
    s.ChildRounding       = 6.0f;
    s.FrameRounding       = 4.0f;
    s.PopupRounding       = 6.0f;
    s.ScrollbarRounding   = 12.0f;
    s.GrabRounding        = 4.0f;
    s.TabRounding         = 4.0f;

    s.WindowBorderSize    = 0.0f;
    s.ChildBorderSize     = 1.0f;
    s.FrameBorderSize     = 0.0f;
    s.PopupBorderSize     = 1.0f;
    s.TabBorderSize       = 0.0f;

    s.WindowTitleAlign    = ImVec2(0.0f, 0.5f);
    s.ButtonTextAlign     = ImVec2(0.5f, 0.5f);
    s.SelectableTextAlign = ImVec2(0.0f, 0.5f);

    s.ScaleAllSizes(dpiScale);
}

} // namespace n64xr::ui::theme
