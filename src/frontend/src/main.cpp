// ============================================================================
//  main.cpp — front door.
// ----------------------------------------------------------------------------
//  Tiny entry: configure spdlog → hand control to UiHost::run().
//  Console exe for now so the log is visible while we iterate.
// ============================================================================

#include "UiHost.h"

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>

int main(int argc, char** argv) {
    // ---- Logger ----------------------------------------------------------
    auto console = spdlog::stdout_color_mt("n64xr");
    spdlog::set_default_logger(console);
    spdlog::set_pattern("[%H:%M:%S.%e] [%^%l%$] %v");
    spdlog::set_level(spdlog::level::info);

    spdlog::info("-------------------------------------------------------------");
    spdlog::info("  N64XR  -  cold-booting the launcher.");
    spdlog::info("-------------------------------------------------------------");

    // ---- Hand off --------------------------------------------------------
    try {
        n64xr::ui::UiHost host;
        const int rc = host.run(argc, argv);
        spdlog::info("N64XR exited cleanly (code {}).", rc);
        return rc;
    } catch (const std::exception& e) {
        spdlog::critical("Fatal: {}", e.what());
        return 1;
    } catch (...) {
        spdlog::critical("Fatal: unknown exception escaped main().");
        return 2;
    }
}
