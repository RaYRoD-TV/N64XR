// ============================================================================
//  SettingsStore.cpp — JSON persistence for AppState (nlohmann/json).
// ============================================================================

#include "SettingsStore.h"
#include "AppState.h"

#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

#include <cstdlib>
#include <filesystem>
#include <fstream>

namespace n64xr::ui {
namespace fs = std::filesystem;

namespace {

fs::path ConfigDir() {
    // %APPDATA%\N64XR  (falls back to temp if APPDATA is somehow unset).
    if (const char* appdata = std::getenv("APPDATA"))
        return fs::path(appdata) / "N64XR";
    return fs::temp_directory_path() / "N64XR";
}

fs::path ConfigFile() { return ConfigDir() / "settings.json"; }

} // namespace

void LoadSettings(AppState& s) {
    std::error_code ec;
    const fs::path file = ConfigFile();
    if (!fs::exists(file, ec)) {
        spdlog::info("Settings: no file yet ({}), using defaults.", file.string());
        return;
    }
    try {
        std::ifstream in(file);
        nlohmann::json j;
        in >> j;
        if (j.contains("romScanPath"))   s.romScanPath   = j["romScanPath"].get<std::string>();
        if (j.contains("coldLight"))     s.coldLight     = j["coldLight"].get<bool>();
        if (j.contains("warmthDial"))    s.warmthDial    = j["warmthDial"].get<float>();
        if (j.contains("cabinetHum"))    s.cabinetHum    = j["cabinetHum"].get<float>();
        if (j.contains("showAliveIdle")) s.showAliveIdle = j["showAliveIdle"].get<bool>();
        if (j.contains("showScanlines")) s.showScanlines = j["showScanlines"].get<bool>();
        if (j.contains("targetFps"))     s.targetFps     = j["targetFps"].get<int>();
        spdlog::info("Settings: loaded from {} (romScanPath='{}').",
                     file.string(), s.romScanPath);
    } catch (const std::exception& e) {
        spdlog::warn("Settings: failed to parse {} ({}); using defaults.",
                     file.string(), e.what());
    }
}

void SaveSettings(const AppState& s) {
    std::error_code ec;
    fs::create_directories(ConfigDir(), ec);

    nlohmann::json j;
    j["romScanPath"]   = s.romScanPath;
    j["coldLight"]     = s.coldLight;
    j["warmthDial"]    = s.warmthDial;
    j["cabinetHum"]    = s.cabinetHum;
    j["showAliveIdle"] = s.showAliveIdle;
    j["showScanlines"] = s.showScanlines;
    j["targetFps"]     = s.targetFps;

    try {
        std::ofstream out(ConfigFile(), std::ios::trunc);
        out << j.dump(2);
        spdlog::info("Settings: saved to {}.", ConfigFile().string());
    } catch (const std::exception& e) {
        spdlog::warn("Settings: failed to save ({}).", e.what());
    }
}

} // namespace n64xr::ui
