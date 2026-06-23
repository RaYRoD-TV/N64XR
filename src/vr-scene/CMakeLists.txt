add_library(cartridgexr_vr_scene STATIC
    src/XrSession.cpp
)
add_library(CartridgeXR::VrScene ALIAS cartridgexr_vr_scene)

target_include_directories(cartridgexr_vr_scene
    PUBLIC
        "${CMAKE_CURRENT_LIST_DIR}/include"
)

target_link_libraries(cartridgexr_vr_scene
    PUBLIC
        CartridgeXR::Common
        Vulkan::Headers
        OpenXR::Headers
    PRIVATE
        openxr_loader
        GPUOpen::VulkanMemoryAllocator
)

target_compile_definitions(cartridgexr_vr_scene
    PUBLIC
        XR_USE_GRAPHICS_API_VULKAN=1
        XR_USE_PLATFORM_WIN32=1
        VK_USE_PLATFORM_WIN32_KHR=1
)
