// ============================================================================
//  SettingsStore.h — persist a slice of AppState across boots.
// ----------------------------------------------------------------------------
//  Writes %APPDATA%\N64XR\settings.json. Load on startup, save after a scan
//  and on exit. Missing / corrupt file => silently fall back to defaults.
// ============================================================================
#pragma once

namespace n64xr::ui {

struct AppState;

// Populate persisted fields from disk (no-op if no file yet).
void LoadSettings(AppState& state);

// Write persisted fields to disk (creates the dir if needed).
void SaveSettings(const AppState& state);

} // namespace n64xr::ui
