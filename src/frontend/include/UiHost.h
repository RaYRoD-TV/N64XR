// ============================================================================
//  UiHost.h — owns the launcher's GLFW window, Vulkan device, ImGui context.
// ----------------------------------------------------------------------------
//  Single responsibility: keep ImGui breathing on a window. Screens know
//  nothing about Vulkan; the AliveIdle layer knows nothing about the
//  swapchain (it draws into the current ImDrawList, so the same code paths
//  later render into a VkImage for the in-VR diegetic panel — Workstream G).
// ============================================================================
#pragma once

#include "AppState.h"

namespace n64xr::ui {

class UiHost {
public:
    UiHost();
    ~UiHost();

    UiHost(const UiHost&)            = delete;
    UiHost& operator=(const UiHost&) = delete;

    // Boots GLFW + Vulkan + ImGui, runs the main loop until the user closes
    // the window or hits the "Power Down" menu entry. Returns a shell-style
    // exit code (0 = clean shutdown).
    int run(int argc, char** argv);

private:
    struct Impl;
    Impl* m_impl = nullptr;

    AppState m_state;
};

} // namespace n64xr::ui
