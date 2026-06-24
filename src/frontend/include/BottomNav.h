// ============================================================================
//  BottomNav.h — the persistent cyan HUD icon rail.
// ----------------------------------------------------------------------------
//  GAMES / SETTINGS / CONTROLLER / VR OPTIONS, each a hand-drawn vector icon +
//  label. The active section gets a filled cyan highlight + underline + bright
//  label; inactive are dim. Writes AppState.currentScreen and returns it.
// ============================================================================
#pragma once

namespace n64xr::ui {

struct AppState;
enum class Screen;

Screen DrawBottomNav(AppState& state, float time);

} // namespace n64xr::ui
