add_library(gfx_vr SHARED
    src/Plugin.cpp
)

set_target_properties(gfx_vr PROPERTIES
    OUTPUT_NAME "mupen64plus-video-n64xr"   # M64P frontends pick plugins by filename pattern
    PREFIX ""                                     # no "lib" prefix on Windows
)

target_include_directories(gfx_vr
    PRIVATE
        "${CMAKE_CURRENT_LIST_DIR}/include"
        "${CMAKE_CURRENT_LIST_DIR}/../../../vendor/mupen64plus-core/src/api"
)

target_link_libraries(gfx_vr
    PRIVATE
        N64XR::Common
        Vulkan::Headers
        GPUOpen::VulkanMemoryAllocator
        OpenXR::Headers
        openxr_loader
)

target_compile_definitions(gfx_vr
    PRIVATE
        M64P_PLUGIN_PROTOTYPES=1
)

# Emit into bin/ alongside N64XR.exe so the frontend can load it by relative path.
set_target_properties(gfx_vr PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}"
)
