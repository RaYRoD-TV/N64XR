// ============================================================================
//  AppState.h — the single mutable bag of state the screens share.
// ----------------------------------------------------------------------------
//  Kept POD-ish on purpose: ImGui draws based on what's in here, nothing more.
// ============================================================================
#pragma once

#include <chrono>
#include <filesystem>
#include <string>
#include <vector>

namespace n64xr::ui {

enum class Screen {
    Home,           // the lobby — "Step Into the Room"
    CartridgeVault, // ROM browser
    ServiceHatch,   // settings
};

struct RomEntry {
    std::filesystem::path path;
    std::string           displayName;
    std::uintmax_t        sizeBytes = 0;
    std::string           extension; // .n64 / .v64 / .z64
};

struct AppState {
    // -- Navigation ---------------------------------------------------------
    Screen currentScreen = Screen::Home;

    // -- Status line (phosphor strip on every screen) -----------------------
    std::string statusLine     = "Awaiting cartridge.";
    std::string openxrRuntime  = "OpenXR runtime: not yet probed.";
    bool        headsetStandingBy = true;

    // -- Cartridge Vault ----------------------------------------------------
    std::string             romScanPath   = "C:/Roms/N64";
    std::vector<RomEntry>   roms;
    int                     selectedRom   = -1;
    bool                    needsRescan   = true;

    // -- Service Hatch (settings placeholders) ------------------------------
    bool   coldLight       = true;   // "Cold Light" toggle = dark theme on
    float  warmthDial      = 0.35f;  // future: amber-bias the scanlines
    float  cabinetHum      = 0.20f;  // future: ambient audio gain
    bool   showAliveIdle   = true;
    bool   showScanlines   = true;
    int    targetFps       = 90;

    // -- VR session bookkeeping (driven by HomeScreen) ----------------------
    bool        vrSessionInFlight = false;
    std::string lastSmokeTestNote = "";

    // -- Timekeeping --------------------------------------------------------
    std::chrono::steady_clock::time_point startTime =
        std::chrono::steady_clock::now();

    float elapsedSeconds() const {
        using namespace std::chrono;
        return duration<float>(steady_clock::now() - startTime).count();
    }
};

} // namespace n64xr::ui
