// ============================================================================
//  Carousel.h — coverflow of holographic cartridge cards from AppState.roms.
// ----------------------------------------------------------------------------
//  Pure ImDrawList + invisible buttons. A single eased scroll scalar glides
//  toward the integer selection. The hero (centre) slot is left TRANSPARENT so
//  the 3D HoloStage cartridge reads through it as the centerpiece; side cards
//  are painted, fanned, dimmed. Arrow/gamepad keys + mouse wheel + click set
//  AppState.selectedRom.
// ============================================================================
#pragma once

#include <imgui.h>

namespace n64xr::ui {

struct AppState;

// Draw the coverflow into [a,b] (screen space). Reads + writes
// state.selectedRom. Returns true if the user activated the hero (click /
// Enter / gamepad-A) — caller may launch.
bool DrawCarousel(AppState& state, ImVec2 a, ImVec2 b, float time);

} // namespace n64xr::ui
