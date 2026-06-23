cmake_minimum_required(VERSION 3.24)

project(CartridgeXR
    VERSION 0.0.1
    DESCRIPTION "Standalone OpenXR-backed VR fork of simple64 + GLideN64"
    LANGUAGES C CXX
)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
    add_compile_options(/W4 /permissive- /Zc:__cplusplus /utf-8 /MP)
    add_compile_definitions(NOMINMAX WIN32_LEAN_AND_MEAN _CRT_SECURE_NO_WARNINGS)
endif()

# Output layout — everything ends up in bin/ next to a single CartridgeXR.exe.
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")

# --- Options -----------------------------------------------------------------
option(CARTRIDGEXR_BUILD_FRONTEND  "Build the Qt6 frontend executable"           ON)
option(CARTRIDGEXR_BUILD_GFX_VR    "Build the OpenXR-backed GFX plugin"          ON)
option(CARTRIDGEXR_BUILD_AUDIO     "Build the Steam-Audio-backed audio plugin"   OFF) # Phase 1a workstream D
option(CARTRIDGEXR_BUILD_INPUT     "Build the OpenXR motion-controller input plugin" OFF) # workstream E
option(CARTRIDGEXR_BUILD_VENDORED  "Build vendored mupen64plus-core / GLideN64 / parallel-rdp / parallel-rsp"
                                   OFF) # Off by default — vendor builds need MSVC patches; opt in once those land.

# --- Dependencies ------------------------------------------------------------

find_package(Vulkan        REQUIRED)
find_package(VulkanHeaders CONFIG REQUIRED)
find_package(VulkanMemoryAllocator CONFIG REQUIRED)
find_package(spdlog        CONFIG REQUIRED)
find_package(fmt           CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(glslang       CONFIG REQUIRED)
find_package(SampleRate    CONFIG REQUIRED)
find_package(ZLIB                 REQUIRED)

# Qt is only needed for the frontend.
if(CARTRIDGEXR_BUILD_FRONTEND)
    find_package(Qt6 6.7 REQUIRED COMPONENTS Core Gui Widgets)
    qt_standard_project_setup()
endif()

# OpenXR lives at C:/dev/OpenXR-SDK (override with -DOPENXR_SDK_ROOT=...).
find_package(OpenXR REQUIRED)

# Steam Audio lives at C:/SDK/steamaudio (override with -DSTEAM_AUDIO_ROOT=...).
# Required only by the audio plugin — gate so a developer focused on rendering
# isn't blocked if Steam Audio isn't installed yet.
if(CARTRIDGEXR_BUILD_AUDIO)
    find_package(SteamAudio REQUIRED)
endif()

# OpenXR loader needs to be built from source.
add_subdirectory("${OPENXR_SDK_ROOT}" "${CMAKE_BINARY_DIR}/openxr_loader_build" EXCLUDE_FROM_ALL)

# --- Targets -----------------------------------------------------------------

add_subdirectory(src/common)
add_subdirectory(src/vr-scene)

if(CARTRIDGEXR_BUILD_GFX_VR)
    add_subdirectory(src/plugins/gfx-vr)
endif()

if(CARTRIDGEXR_BUILD_AUDIO)
    add_subdirectory(src/plugins/audio-spatial)
endif()

if(CARTRIDGEXR_BUILD_INPUT)
    add_subdirectory(src/plugins/input-xr)
endif()

if(CARTRIDGEXR_BUILD_FRONTEND)
    add_subdirectory(src/frontend)
endif()

# --- Vendored emulator core + RDP/RSP (opt-in until MSVC patches land) ------
if(CARTRIDGEXR_BUILD_VENDORED)
    message(STATUS "CartridgeXR: vendored builds enabled (requires MSVC patches in cmake/patches/).")
    # Slot for: add_subdirectory(vendor/mupen64plus-core)
    # Slot for: add_subdirectory(vendor/GLideN64)
    # Slot for: add_subdirectory(vendor/parallel-rdp-standalone)
    # Slot for: add_subdirectory(vendor/parallel-rsp)
endif()

# --- Status banner -----------------------------------------------------------
message(STATUS "----------------------------------------------------------------")
message(STATUS "CartridgeXR ${CartridgeXR_VERSION}")
message(STATUS "  Frontend (Qt6):     ${CARTRIDGEXR_BUILD_FRONTEND}")
message(STATUS "  GFX-VR plugin:      ${CARTRIDGEXR_BUILD_GFX_VR}")
message(STATUS "  Audio plugin:       ${CARTRIDGEXR_BUILD_AUDIO}")
message(STATUS "  Input plugin:       ${CARTRIDGEXR_BUILD_INPUT}")
message(STATUS "  Vendored cores:     ${CARTRIDGEXR_BUILD_VENDORED}")
message(STATUS "  Vulkan SDK:         ${Vulkan_VERSION}")
message(STATUS "  OpenXR SDK root:    ${OPENXR_SDK_ROOT}")
if(CARTRIDGEXR_BUILD_AUDIO)
message(STATUS "  Steam Audio root:   ${STEAM_AUDIO_ROOT}")
endif()
message(STATUS "----------------------------------------------------------------")
