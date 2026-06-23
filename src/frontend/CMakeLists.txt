# ============================================================================
#  N64XR — frontend (Dear ImGui launcher shell)
# ----------------------------------------------------------------------------
#  GLFW window + Vulkan instance/device/swapchain + ImGui.
#  Console exe for now (WIN32_EXECUTABLE OFF) so spdlog stays visible.
# ============================================================================

add_executable(n64xr_frontend
    src/main.cpp
    src/UiHost.cpp
    src/Theme.cpp
    src/AliveIdle.cpp
    src/HomeScreen.cpp
    src/RomBrowserScreen.cpp
    src/SettingsScreen.cpp
)

add_executable(N64XR::Frontend ALIAS n64xr_frontend)

set_target_properties(n64xr_frontend PROPERTIES
    OUTPUT_NAME "n64xr"
    WIN32_EXECUTABLE OFF        # keep the console attached during dev
)

target_include_directories(n64xr_frontend
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

target_link_libraries(n64xr_frontend
    PRIVATE
        N64XR::Common
        N64XR::VrScene
        imgui::imgui                # backends included via vcpkg port features
        glfw
        Vulkan::Vulkan
        spdlog::spdlog
        fmt::fmt
)

target_compile_definitions(n64xr_frontend PRIVATE
    GLFW_INCLUDE_VULKAN
    IMGUI_DEFINE_MATH_OPERATORS
    # Match vr-scene PUBLIC defines so HomeScreen.cpp's #include "XrSession.h"
    # sees the OpenXR Vulkan structs in their complete form regardless of
    # include order in any TU that pulls XrSession.h transitively.
    XR_USE_GRAPHICS_API_VULKAN=1
)

if (MSVC)
    target_compile_options(n64xr_frontend PRIVATE /bigobj)
endif()
