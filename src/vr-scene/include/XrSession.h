#pragma once

// Minimal OpenXR + Vulkan session that submits a magenta clear in both eyes.
// Validates the toolchain end-to-end before any GLideN64-derived rendering exists.
// Replaced in Phase 1a workstream C with a Vulkan-rendered CRT room scene.

#define XR_USE_GRAPHICS_API_VULKAN 1
// Deliberately NOT defining XR_USE_PLATFORM_WIN32 — it pulls in MSFT
// perception-anchor + holographic-window-attachment structs that need
// IUnknown (COM), which isn't worth the dependency for the Phase 1
// smoke test. The Win32-perf-counter extension we used to list is
// dropped too (it was unused).
//
// Order matters: openxr_platform.h references VkInstance/VkDevice/etc
// inside its Vulkan conditional structs — vulkan.h must be visible
// BEFORE the platform header is parsed.
#include <vulkan/vulkan.h>
#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>

#include <cstdint>
#include <vector>

namespace n64xr {

class XrSession {
public:
    XrSession();
    ~XrSession();

    XrSession(const XrSession&)            = delete;
    XrSession& operator=(const XrSession&) = delete;

    // Brings up the OpenXR instance, picks a system, creates a Vulkan device
    // matching the runtime's requirements, opens an XrSession, and creates
    // per-eye swapchains. Returns false (and logs the offender) on any failure.
    bool initialize();

    // Pumps one frame: poll events → wait/begin → magenta clear in each eye → end.
    // Returns false when the OpenXR session has asked to exit (the caller should
    // tear down).
    bool pumpFrame();

    void shutdown();

private:
    bool createXrInstance();
    bool acquireSystem();
    bool createVulkanDeviceForXr();
    bool createXrSession();
    bool createSwapchains();
    void pollEvents(bool& exitRequested);
    void renderFrame();

    void clearImageMagenta(VkCommandBuffer cmd, VkImage image,
                           uint32_t width, uint32_t height,
                           VkImageLayout initialLayout, VkImageLayout finalLayout);

    XrInstance     m_instance     {XR_NULL_HANDLE};
    XrSystemId     m_systemId     {XR_NULL_SYSTEM_ID};
    ::XrSession    m_session      {XR_NULL_HANDLE};
    XrSpace        m_appSpace     {XR_NULL_HANDLE};
    XrSessionState m_sessionState {XR_SESSION_STATE_UNKNOWN};
    bool           m_sessionRunning{false};

    VkInstance       m_vkInstance       {VK_NULL_HANDLE};
    VkPhysicalDevice m_vkPhysicalDevice {VK_NULL_HANDLE};
    VkDevice         m_vkDevice         {VK_NULL_HANDLE};
    uint32_t         m_vkQueueFamily    {UINT32_MAX};
    VkQueue          m_vkQueue          {VK_NULL_HANDLE};
    VkCommandPool    m_vkCommandPool    {VK_NULL_HANDLE};

    struct EyeSwapchain {
        XrSwapchain swapchain {XR_NULL_HANDLE};
        int32_t     width     {0};
        int32_t     height    {0};
        int64_t     format    {0};
        std::vector<XrSwapchainImageVulkanKHR> images;
    };
    std::vector<EyeSwapchain>            m_swapchains;
    std::vector<XrViewConfigurationView> m_viewConfigs;
};

}  // namespace n64xr
