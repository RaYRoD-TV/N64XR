// ============================================================================
//  Screens.h — free functions, one per launcher tab.
// ----------------------------------------------------------------------------
//  Every screen is reentrant: takes the shared AppState, draws into the
//  *current* ImGui window. UiHost decides the host window; screens decide
//  their own contents.
// ============================================================================
#pragma once

namespace n64xr::ui {

struct AppState;

void DrawHomeScreen       (AppState& state);
void DrawRomBrowserScreen (AppState& state);   // "Cartridge Vault"
void DrawSettingsScreen   (AppState& state);   // "Service Hatch"

// Scan AppState::romScanPath into AppState::roms (also persists the path).
// Callable from any screen / startup so the home carousel populates.
void ScanRoms(AppState& state);

// Shared helper: the breathing primary-action button. Returns true on click.
bool BreathingButton(const char* label, float widthScale = 1.0f);

// Shared helper: the phosphor strip drawn at the bottom of every screen.
void DrawPhosphorStatusStrip(const AppState& state);

} // namespace n64xr::ui
