// ============================================================================
//  Theme.h — "a console that arrived in a parcel from the 1970s."
// ----------------------------------------------------------------------------
//  Aged-paper warm whites on deep navy, brass/copper accents, phosphor green
//  for status text. Display = Orbitron, body = JetBrains Mono, status = VT323.
//  All knobs collected here so the rest of the launcher is colour-agnostic.
// ============================================================================
#pragma once

#include <imgui.h>

namespace n64xr::ui::theme {

// ---- Palette --------------------------------------------------------------
struct Palette {
    ImVec4 deepNavyBg       {0.043f, 0.063f, 0.102f, 1.00f}; // #0B101A
    ImVec4 panelNavy        {0.071f, 0.102f, 0.157f, 1.00f}; // #121A28
    ImVec4 panelNavyRaised  {0.102f, 0.141f, 0.220f, 1.00f}; // #1A2438

    ImVec4 brass            {0.788f, 0.604f, 0.239f, 1.00f}; // #C99A3D
    ImVec4 brassHot         {0.910f, 0.722f, 0.337f, 1.00f}; // #E8B856
    ImVec4 brassDim         {0.549f, 0.420f, 0.153f, 1.00f}; // #8C6B27
    ImVec4 copper           {0.725f, 0.478f, 0.282f, 1.00f}; // #B97A48

    ImVec4 agedPaper        {0.910f, 0.863f, 0.769f, 1.00f}; // #E8DCC4
    ImVec4 agedPaperDim     {0.722f, 0.675f, 0.580f, 1.00f}; // #B8AC94

    ImVec4 phosphor         {0.486f, 0.890f, 0.545f, 1.00f}; // #7CE38B
    ImVec4 phosphorDim      {0.227f, 0.478f, 0.267f, 1.00f}; // #3A7A44

    ImVec4 borderWarm       {0.235f, 0.180f, 0.102f, 0.533f}; // #3C2E1A88
    ImVec4 alertWarm        {0.847f, 0.400f, 0.180f, 1.00f}; // #D8662E
};

// ---- Font handles ---------------------------------------------------------
struct Fonts {
    ImFont* display      = nullptr; // Orbitron, ~34pt
    ImFont* displaySmall = nullptr; // Orbitron, ~20pt
    ImFont* body         = nullptr; // JetBrains Mono, ~16pt
    ImFont* bodySmall    = nullptr; // JetBrains Mono, ~13pt
    ImFont* phosphor     = nullptr; // VT323, ~22pt
};

// One-shot calls invoked by UiHost. Safe to call ApplyStyle() again later
// to refresh after a DPI change.
void LoadFonts(float dpiScale);   // call BEFORE ImGui_ImplVulkan_Init()
void ApplyStyle(float dpiScale);  // colours + spacing + rounding

const Palette& Colours();
const Fonts&   GetFonts();

} // namespace n64xr::ui::theme
