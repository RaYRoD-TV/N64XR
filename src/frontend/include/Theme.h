// ============================================================================
//  Theme.h — premium CYAN / TEAL HOLOGRAPHIC VR HUD palette + typography.
// ----------------------------------------------------------------------------
//  Luminous cyan light over a near-black navy void. The Palette field NAMES
//  are kept identical to the previous (brass-era) layout so every other TU
//  still compiles — but their VALUES are now the cyan scheme. New, clearly
//  named cyan fields are appended for fresh code that wants intent, not legacy.
//  Display = Orbitron, body = JetBrains Mono, status = VT323.
// ============================================================================
#pragma once

#include <imgui.h>

namespace n64xr::ui::theme {

// ---- Palette --------------------------------------------------------------
struct Palette {
    // ----- LEGACY NAMES (repurposed to cyan; keep so old code compiles) -----
    ImVec4 deepNavyBg       {0.016f, 0.031f, 0.059f, 1.00f}; // #04080F  void
    ImVec4 panelNavy        {0.039f, 0.086f, 0.133f, 0.85f}; // #0A1622  translucent
    ImVec4 panelNavyRaised  {0.063f, 0.145f, 0.212f, 0.94f}; // #102536  raised

    ImVec4 brass            {0.157f, 0.902f, 0.941f, 1.00f}; // #28E6F0  primary cyan
    ImVec4 brassHot         {0.490f, 0.976f, 1.000f, 1.00f}; // #7DF9FF  hot/glow core
    ImVec4 brassDim         {0.118f, 0.431f, 0.471f, 1.00f}; // #1E6E78  dim cyan
    ImVec4 copper           {0.157f, 0.902f, 0.941f, 1.00f}; // = cyan

    ImVec4 agedPaper        {0.918f, 0.984f, 1.000f, 1.00f}; // #EAFBFF  cool white
    ImVec4 agedPaperDim     {0.431f, 0.580f, 0.627f, 1.00f}; // #6E94A0  dim label

    ImVec4 phosphor         {0.231f, 1.000f, 0.627f, 1.00f}; // #3BFFA0  active green
    ImVec4 phosphorDim      {0.137f, 0.502f, 0.337f, 1.00f}; // #238056  dim green

    ImVec4 borderWarm       {0.157f, 0.902f, 0.941f, 0.667f}; // cyan @ 0.67
    ImVec4 alertWarm        {1.000f, 0.698f, 0.243f, 1.00f}; // #FFB23E  amber warn

    // ----- NEW, CLEARLY-NAMED CYAN FIELDS (preferred by new code) ----------
    ImVec4 voidBg           {0.016f, 0.031f, 0.059f, 1.00f}; // #04080F
    ImVec4 cyan             {0.157f, 0.902f, 0.941f, 1.00f}; // #28E6F0
    ImVec4 cyanHot          {0.490f, 0.976f, 1.000f, 1.00f}; // #7DF9FF
    ImVec4 cyanDim          {0.118f, 0.431f, 0.471f, 1.00f}; // #1E6E78
    ImVec4 gridLine         {0.118f, 0.431f, 0.471f, 0.33f}; // dim cyan, low alpha
    ImVec4 coolWhite        {0.918f, 0.984f, 1.000f, 1.00f}; // #EAFBFF
    ImVec4 coolWhiteDim     {0.431f, 0.580f, 0.627f, 1.00f}; // #6E94A0
    ImVec4 activeGreen      {0.231f, 1.000f, 0.627f, 1.00f}; // #3BFFA0
    ImVec4 warnAmber        {1.000f, 0.698f, 0.243f, 1.00f}; // #FFB23E
};

// ---- Font handles ---------------------------------------------------------
struct Fonts {
    ImFont* display      = nullptr; // Orbitron, ~36pt
    ImFont* displaySmall = nullptr; // Orbitron, ~20pt
    ImFont* body         = nullptr; // JetBrains Mono, ~16pt
    ImFont* bodySmall    = nullptr; // JetBrains Mono, ~13pt
    ImFont* phosphor     = nullptr; // VT323, ~22pt
};

void LoadFonts(float dpiScale);   // call BEFORE ImGui_ImplVulkan_Init()
void ApplyStyle(float dpiScale);  // colours + spacing + rounding

const Palette& Colours();
const Fonts&   GetFonts();

} // namespace n64xr::ui::theme
