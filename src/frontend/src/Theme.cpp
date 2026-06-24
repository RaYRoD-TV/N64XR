// ============================================================================
//  Theme.cpp — cyan/teal holographic palette + typography + angular HUD style.
// ----------------------------------------------------------------------------
//  Fonts are looked up under <exe-dir>/assets/fonts/; if any are missing we
//  log a warning and fall back to the built-in proggy so the launcher boots.
//  The style keeps WindowBg/ChildBg translucent and pushes the energy into
//  cyan borders — that is what reads as "light projected onto glass."
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

    if (!g_fonts.body) {
        spdlog::warn("Theme: JetBrainsMono missing — using ImGui default as body face.");
        g_fonts.body = atlas->Fonts.back();
    }
    // ImGui 1.92 bakes font textures lazily on first use.
}

// ---------------------------------------------------------------------------
//  Style — cyan HUD palette + angular spacing + minimal rounding.
// ---------------------------------------------------------------------------
void ApplyStyle(float dpiScale) {
    ImGuiStyle& s = ImGui::GetStyle();
    ImVec4* c     = s.Colors;
    const Palette& p = g_palette;

    auto mul = [](ImVec4 col, float k) {
        return ImVec4(col.x * k, col.y * k, col.z * k, col.w);
    };
    auto with_alpha = [](ImVec4 col, float a) {
        return ImVec4(col.x, col.y, col.z, a);
    };

    c[ImGuiCol_Text]                  = p.coolWhite;
    c[ImGuiCol_TextDisabled]          = p.coolWhiteDim;
    c[ImGuiCol_TextSelectedBg]        = with_alpha(p.cyan, 0.40f);

    c[ImGuiCol_WindowBg]              = p.panelNavy;
    c[ImGuiCol_ChildBg]               = with_alpha(p.panelNavy, 0.75f);
    c[ImGuiCol_PopupBg]               = with_alpha(p.panelNavyRaised, 0.96f);
    c[ImGuiCol_MenuBarBg]             = p.panelNavyRaised;

    c[ImGuiCol_Border]                = with_alpha(p.cyan, 0.55f);
    c[ImGuiCol_BorderShadow]          = ImVec4(0,0,0,0);

    c[ImGuiCol_FrameBg]               = with_alpha(p.panelNavyRaised, 0.75f);
    c[ImGuiCol_FrameBgHovered]        = with_alpha(p.cyan, 0.22f);
    c[ImGuiCol_FrameBgActive]         = with_alpha(p.cyanDim, 1.00f);

    c[ImGuiCol_TitleBg]               = p.panelNavy;
    c[ImGuiCol_TitleBgActive]         = p.panelNavyRaised;
    c[ImGuiCol_TitleBgCollapsed]      = p.panelNavy;

    c[ImGuiCol_Button]                = with_alpha(p.cyan,    0.14f);
    c[ImGuiCol_ButtonHovered]         = with_alpha(p.cyanHot, 0.45f);
    c[ImGuiCol_ButtonActive]          = mul(p.cyanHot, 0.80f);

    c[ImGuiCol_Header]                = with_alpha(p.cyan, 0.18f);
    c[ImGuiCol_HeaderHovered]         = with_alpha(p.cyan, 0.40f);
    c[ImGuiCol_HeaderActive]          = with_alpha(p.cyanHot, 0.55f);

    c[ImGuiCol_CheckMark]             = p.activeGreen;
    c[ImGuiCol_SliderGrab]            = p.cyan;
    c[ImGuiCol_SliderGrabActive]      = p.cyanHot;

    c[ImGuiCol_Tab]                   = with_alpha(p.panelNavy, 1.0f);
    c[ImGuiCol_TabHovered]            = with_alpha(p.cyan, 0.45f);
    c[ImGuiCol_TabActive]             = with_alpha(p.cyan, 0.30f);
    c[ImGuiCol_TabUnfocused]          = p.panelNavy;
    c[ImGuiCol_TabUnfocusedActive]    = with_alpha(p.cyanDim, 0.40f);

    c[ImGuiCol_Separator]             = with_alpha(p.cyanDim, 0.55f);
    c[ImGuiCol_SeparatorHovered]      = p.cyan;
    c[ImGuiCol_SeparatorActive]       = p.cyanHot;

    c[ImGuiCol_ScrollbarBg]           = with_alpha(p.voidBg, 0.0f);
    c[ImGuiCol_ScrollbarGrab]         = with_alpha(p.cyanDim, 0.65f);
    c[ImGuiCol_ScrollbarGrabHovered]  = p.cyan;
    c[ImGuiCol_ScrollbarGrabActive]   = p.cyanHot;

    c[ImGuiCol_ResizeGrip]            = with_alpha(p.cyanDim, 0.30f);
    c[ImGuiCol_ResizeGripHovered]     = p.cyan;
    c[ImGuiCol_ResizeGripActive]      = p.cyanHot;

    c[ImGuiCol_NavHighlight]          = p.cyanHot;
    c[ImGuiCol_NavWindowingHighlight] = with_alpha(p.cyanHot, 0.85f);
    c[ImGuiCol_NavWindowingDimBg]     = ImVec4(0,0,0,0.40f);
    c[ImGuiCol_ModalWindowDimBg]      = ImVec4(0,0,0,0.55f);

    c[ImGuiCol_PlotLines]             = p.cyan;
    c[ImGuiCol_PlotLinesHovered]      = p.cyanHot;
    c[ImGuiCol_PlotHistogram]         = p.cyanDim;
    c[ImGuiCol_PlotHistogramHovered]  = p.cyanHot;

    // ---- Geometry — angular HUD, never balloon-y ------------------------
    s.WindowPadding       = ImVec2(18, 16);
    s.FramePadding        = ImVec2(12,  7);
    s.CellPadding         = ImVec2( 8,  4);
    s.ItemSpacing         = ImVec2(10,  8);
    s.ItemInnerSpacing    = ImVec2( 6,  4);
    s.IndentSpacing       = 22.0f;
    s.ScrollbarSize       = 14.0f;
    s.GrabMinSize         = 12.0f;

    s.WindowRounding      = 0.0f;
    s.ChildRounding       = 2.0f;
    s.FrameRounding       = 2.0f;
    s.PopupRounding       = 2.0f;
    s.ScrollbarRounding   = 8.0f;
    s.GrabRounding        = 2.0f;
    s.TabRounding         = 2.0f;

    s.WindowBorderSize    = 0.0f;
    s.ChildBorderSize     = 1.0f;
    s.FrameBorderSize     = 1.0f;
    s.PopupBorderSize     = 1.0f;
    s.TabBorderSize       = 0.0f;

    s.WindowTitleAlign    = ImVec2(0.0f, 0.5f);
    s.ButtonTextAlign     = ImVec2(0.5f, 0.5f);
    s.SelectableTextAlign = ImVec2(0.0f, 0.5f);

    s.ScaleAllSizes(dpiScale);
}

} // namespace n64xr::ui::theme
