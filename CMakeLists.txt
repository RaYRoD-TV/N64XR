# ============================================================================
#  N64XR — root build
# ----------------------------------------------------------------------------
#  MSVC 2026 / Windows / vcpkg toolchain.
#  Vulkan + OpenXR + Dear ImGui + GLFW launcher.
# ============================================================================
cmake_minimum_required(VERSION 3.26)

project(N64XR
    VERSION 0.1.0
    DESCRIPTION "N64XR — Nintendo 64 in VR"
    LANGUAGES C CXX)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if (MSVC)
    add_compile_options(/W4 /permissive- /Zc:__cplusplus /utf-8 /MP)
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS NOMINMAX WIN32_LEAN_AND_MEAN)
endif()

# Output layout — keep all binaries in bin/ next to a single n64xr.exe.
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")

# Plugin build flags retained from earlier scaffold.
option(N64XR_BUILD_GFX_VR    "Build the OpenXR-backed GFX plugin"                  ON)
option(N64XR_BUILD_AUDIO     "Build the Steam-Audio-backed audio plugin"           OFF)
option(N64XR_BUILD_INPUT     "Build the OpenXR motion-controller input plugin"     OFF)
option(N64XR_BUILD_VENDORED  "Build vendored mupen64plus-core / GLideN64 / etc."   OFF)

# ---------------------------------------------------------------------------
#  Dependencies (vcpkg + module-mode finders in cmake/)
# ---------------------------------------------------------------------------
find_package(Vulkan                     REQUIRED)
find_package(VulkanHeaders         CONFIG REQUIRED)
find_package(VulkanMemoryAllocator CONFIG REQUIRED)
find_package(spdlog                CONFIG REQUIRED)
find_package(fmt                   CONFIG REQUIRED)
find_package(nlohmann_json         CONFIG REQUIRED)
find_package(glslang               CONFIG REQUIRED)
find_package(SampleRate            CONFIG REQUIRED)
find_package(ZLIB                          REQUIRED)

# New for the launcher (Workstream A — Dear ImGui front-end):
find_package(imgui                 CONFIG REQUIRED)
find_package(glfw3                 CONFIG REQUIRED)

# OpenXR lives at C:/dev/OpenXR-SDK (override with -DOPENXR_SDK_ROOT=...).
# Resolved via cmake/FindOpenXR.cmake (MODULE mode), NOT a vcpkg port.
find_package(OpenXR REQUIRED)

# Steam Audio is gated behind the audio plugin so a renderer-focused dev
# isn't blocked when it isn't installed yet.
if(N64XR_BUILD_AUDIO)
    find_package(SteamAudio REQUIRED)
endif()

# OpenXR loader is built from source as a subdirectory target ("openxr_loader").
add_subdirectory("${OPENXR_SDK_ROOT}" "${CMAKE_BINARY_DIR}/openxr_loader_build" EXCLUDE_FROM_ALL)

# ---------------------------------------------------------------------------
#  Modules
# ---------------------------------------------------------------------------
add_subdirectory(src/common)
add_subdirectory(src/vr-scene)
add_subdirectory(src/frontend)

if(N64XR_BUILD_GFX_VR)
    add_subdirectory(src/plugins/gfx-vr)
endif()
if(N64XR_BUILD_AUDIO)
    add_subdirectory(src/plugins/audio-spatial)
endif()
if(N64XR_BUILD_INPUT)
    add_subdirectory(src/plugins/input-xr)
endif()

if(N64XR_BUILD_VENDORED)
    message(STATUS "N64XR: vendored builds enabled (requires MSVC patches in cmake/patches/).")
endif()

# ---------------------------------------------------------------------------
#  Status banner
# ---------------------------------------------------------------------------
message(STATUS "----------------------------------------------------------------")
message(STATUS "N64XR ${N64XR_VERSION}")
message(STATUS "  Frontend (ImGui):   ON")
message(STATUS "  GFX-VR plugin:      ${N64XR_BUILD_GFX_VR}")
message(STATUS "  Audio plugin:       ${N64XR_BUILD_AUDIO}")
message(STATUS "  Input plugin:       ${N64XR_BUILD_INPUT}")
message(STATUS "  Vendored cores:     ${N64XR_BUILD_VENDORED}")
message(STATUS "  Vulkan SDK:         ${Vulkan_VERSION}")
message(STATUS "  OpenXR SDK root:    ${OPENXR_SDK_ROOT}")
if(N64XR_BUILD_AUDIO)
message(STATUS "  Steam Audio root:   ${STEAM_AUDIO_ROOT}")
endif()
message(STATUS "----------------------------------------------------------------")

# ---------------------------------------------------------------------------
#  Assets — make the launcher find its fonts at runtime
# ---------------------------------------------------------------------------
set(N64XR_ASSETS_SRC "${CMAKE_SOURCE_DIR}/assets")
if (EXISTS "${N64XR_ASSETS_SRC}")
    add_custom_target(n64xr_copy_assets ALL
        COMMAND ${CMAKE_COMMAND} -E copy_directory
                "${N64XR_ASSETS_SRC}"
                "$<TARGET_FILE_DIR:n64xr_frontend>/assets"
        COMMENT "Mirroring assets/ next to the frontend exe")
    add_dependencies(n64xr_copy_assets n64xr_frontend)
endif()
