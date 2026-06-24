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
    src/HoloStage.cpp
    src/Cartridge.cpp
    src/MeshGLB.cpp
    src/GlassPanel.cpp
    src/SettingsStore.cpp
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
        nlohmann_json::nlohmann_json
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

# ---- Compile GLSL -> SPIR-V next to the exe --------------------------------
#  HoloStage loads "assets/shaders/<name>.spv" at runtime (working dir = exe
#  dir, same convention Theme.cpp uses for fonts). glslc comes from the root
#  find_package(Vulkan).
if (NOT Vulkan_GLSLC_EXECUTABLE)
    get_filename_component(_vk_bin "${Vulkan_INCLUDE_DIR}/../Bin" ABSOLUTE)
    find_program(Vulkan_GLSLC_EXECUTABLE NAMES glslc HINTS "${_vk_bin}")
endif()
if (NOT Vulkan_GLSLC_EXECUTABLE)
    message(FATAL_ERROR "glslc not found — cannot compile HoloStage shaders.")
endif()

set(HOLO_SHADER_SRC_DIR "${CMAKE_SOURCE_DIR}/assets/shaders")
# Compile to a config-agnostic plain dir (genex not allowed in OUTPUT),
# then POST_BUILD-copy next to the exe (genex IS allowed in COMMAND).
set(HOLO_SHADER_SPV_DIR "${CMAKE_BINARY_DIR}/shaders_spv")

set(HOLO_SHADERS
    cartridge.vert
    cartridge.frag
    fullscreen.vert
    brightpass.frag
    blur.frag
    upsample.frag
    composite.frag)

set(HOLO_SPV_FILES "")
foreach(SH ${HOLO_SHADERS})
    set(_src "${HOLO_SHADER_SRC_DIR}/${SH}")
    set(_spv "${HOLO_SHADER_SPV_DIR}/${SH}.spv")
    add_custom_command(
        OUTPUT  "${_spv}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${HOLO_SHADER_SPV_DIR}"
        COMMAND "${Vulkan_GLSLC_EXECUTABLE}" --target-env=vulkan1.3 -O "${_src}" -o "${_spv}"
        DEPENDS "${_src}"
        COMMENT "glslc ${SH}"
        VERBATIM)
    list(APPEND HOLO_SPV_FILES "${_spv}")
endforeach()

add_custom_target(n64xr_compile_shaders ALL DEPENDS ${HOLO_SPV_FILES})
add_dependencies(n64xr_frontend n64xr_compile_shaders)

# Mirror the compiled .spv next to the exe (into assets/shaders/), where
# HoloStage loads them at runtime relative to the working dir.
add_custom_command(TARGET n64xr_frontend POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory
            "$<TARGET_FILE_DIR:n64xr_frontend>/assets/shaders"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${HOLO_SHADER_SPV_DIR}"
            "$<TARGET_FILE_DIR:n64xr_frontend>/assets/shaders"
    COMMENT "Mirroring compiled .spv next to n64xr.exe"
    VERBATIM)
