// N64XR — console smoke-test frontend (Phase 1 scaffold)
//
// No GUI library. Opens an OpenXR session via the vr-scene module and pumps
// frames clearing magenta in both eyes for ~3 seconds, then exits. PASS =
// solid magenta in both eyes of the active HMD via the active OpenXR runtime
// (SteamVR / Oculus / WMR / Quest Link).
//
// Usage:
//   N64XR                    Run the smoke test (~3s magenta clear).
//   N64XR --frames N         Run N frames before exiting (default 270 ≈ 3s @ 90Hz).
//   N64XR --help             Print usage and exit.
//
// The Phase 1a launcher (ROM browser, room scene, settings) replaces this with
// a Vulkan-rendered desktop window and the in-VR diegetic menu (workstream G).

#include "XrSession.h"

#include <spdlog/spdlog.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

namespace {

void printUsage() {
    std::printf(
        "N64XR 0.0.1 - Phase 1 smoke test\n"
        "\n"
        "Usage: N64XR [options]\n"
        "  --frames N    run N frames before exiting (default 270 ~= 3s at 90Hz)\n"
        "  --help        show this help\n"
        "\n"
        "Opens an OpenXR session and clears both eyes magenta.\n"
        "Requires an active OpenXR runtime (SteamVR, Oculus PC, WMR, etc).\n");
}

int parseInt(const char* s, int fallback) {
    char* end = nullptr;
    long v = std::strtol(s, &end, 10);
    if (end == s || *end != '\0') return fallback;
    return static_cast<int>(v);
}

}  // namespace

int main(int argc, char** argv) {
    spdlog::set_level(spdlog::level::debug);
    spdlog::set_pattern("[%H:%M:%S.%e] [%^%l%$] %v");

    int framesToRun = 270;  // ~3s at 90Hz

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--help" || arg == "-h") {
            printUsage();
            return EXIT_SUCCESS;
        }
        if (arg == "--frames" && i + 1 < argc) {
            framesToRun = parseInt(argv[++i], framesToRun);
            continue;
        }
        spdlog::warn("Unrecognised argument: {}", arg);
    }

    spdlog::info("N64XR 0.0.1 smoke test starting (frames={})", framesToRun);

    n64xr::XrSession session;
    if (!session.initialize()) {
        spdlog::error("OpenXR session failed to initialise. Confirm a runtime "
                      "(SteamVR / Oculus PC / WMR) is set as the active OpenXR runtime.");
        return EXIT_FAILURE;
    }

    int framesPumped = 0;
    for (; framesPumped < framesToRun; ++framesPumped) {
        if (!session.pumpFrame()) {
            spdlog::info("Session asked to exit at frame {}", framesPumped);
            break;
        }
    }
    session.shutdown();

    spdlog::info("Smoke test complete. {} frames pumped.", framesPumped);
    return EXIT_SUCCESS;
}
