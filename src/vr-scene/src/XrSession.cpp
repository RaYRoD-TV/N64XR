#include "XrSession.h"

#include <spdlog/spdlog.h>

#include <array>
#include <cstring>
#include <string>
#include <vector>

namespace cxr {
namespace {

constexpr XrViewConfigurationType kViewConfig = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
constexpr XrEnvironmentBlendMode  kBlendMode  = XR_ENVIRONMENT_BLEND_MODE_OPAQUE;

bool xr_check(XrResult result, const char* what) {
    if (XR_SUCCEEDED(result)) return true;
    spdlog::error("[XrSession] {} failed: XrResult={}", what, static_cast<int>(result));
    return false;
}

}  // namespace

XrSession::XrSession()  = default;
XrSession::~XrSession() { shutdown(); }

bool XrSession::initialize() {
    if (!createXrInstance())         return false;
    if (!acquireSystem())            return false;
    if (!createVulkanDeviceForXr())  return false;
    if (!createXrSession())          return false;
    if (!createSwapchains())         return false;
    spdlog::info("[XrSession] initialised");
    return true;
}

bool XrSession::createXrInstance() {
    const std::array<const char*, 2> extensions{
        XR_KHR_VULKAN_ENABLE2_EXTENSION_NAME,
        XR_KHR_WIN32_CONVERT_PERFORMANCE_COUNTER_TIME_EXTENSION_NAME,
    };

    XrInstanceCreateInfo create{XR_TYPE_INSTANCE_CREATE_INFO};
    std::strncpy(create.applicationInfo.applicationName, "CartridgeXR", XR_MAX_APPLICATION_NAME_SIZE - 1);
    create.applicationInfo.applicationVersion = 1;
    std::strncpy(create.applicationInfo.engineName, "CartridgeXR vr-scene", XR_MAX_ENGINE_NAME_SIZE - 1);
    create.applicationInfo.engineVersion = 1;
    create.applicationInfo.apiVersion    = XR_CURRENT_API_VERSION;
    create.enabledExtensionCount         = static_cast<uint32_t>(extensions.size());
    create.enabledExtensionNames         = extensions.data();

    return xr_check(xrCreateInstance(&create, &m_instance), "xrCreateInstance");
}

bool XrSession::acquireSystem() {
    XrSystemGetInfo info{XR_TYPE_SYSTEM_GET_INFO};
    info.formFactor = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;
    if (!xr_check(xrGetSystem(m_instance, &info, &m_systemId), "xrGetSystem")) return false;

    uint32_t viewCount = 0;
    if (!xr_check(xrEnumerateViewConfigurationViews(m_instance, m_systemId, kViewConfig,
                                                    0, &viewCount, nullptr),
                  "xrEnumerateViewConfigurationViews (count)")) return false;
    m_viewConfigs.assign(viewCount, {XR_TYPE_VIEW_CONFIGURATION_VIEW});
    if (!xr_check(xrEnumerateViewConfigurationViews(m_instance, m_systemId, kViewConfig,
                                                    viewCount, &viewCount, m_viewConfigs.data()),
                  "xrEnumerateViewConfigurationViews (data)")) return false;

    spdlog::info("[XrSession] system has {} views", viewCount);
    return true;
}

bool XrSession::createVulkanDeviceForXr() {
    // XR_KHR_vulkan_enable2 dictates instance + device creation parameters.
    PFN_xrGetVulkanGraphicsRequirements2KHR pfnGetReqs = nullptr;
    PFN_xrCreateVulkanInstanceKHR           pfnCreateVkInst = nullptr;
    PFN_xrGetVulkanGraphicsDevice2KHR       pfnGetVkPhys = nullptr;
    PFN_xrCreateVulkanDeviceKHR             pfnCreateVkDev  = nullptr;
    auto loadFn = [&](const char* name, PFN_xrVoidFunction* out) {
        return xr_check(xrGetInstanceProcAddr(m_instance, name, out), name);
    };
    if (!loadFn("xrGetVulkanGraphicsRequirements2KHR", reinterpret_cast<PFN_xrVoidFunction*>(&pfnGetReqs)))     return false;
    if (!loadFn("xrCreateVulkanInstanceKHR",           reinterpret_cast<PFN_xrVoidFunction*>(&pfnCreateVkInst)))return false;
    if (!loadFn("xrGetVulkanGraphicsDevice2KHR",       reinterpret_cast<PFN_xrVoidFunction*>(&pfnGetVkPhys)))   return false;
    if (!loadFn("xrCreateVulkanDeviceKHR",             reinterpret_cast<PFN_xrVoidFunction*>(&pfnCreateVkDev))) return false;

    XrGraphicsRequirementsVulkan2KHR reqs{XR_TYPE_GRAPHICS_REQUIREMENTS_VULKAN2_KHR};
    if (!xr_check(pfnGetReqs(m_instance, m_systemId, &reqs), "xrGetVulkanGraphicsRequirements2KHR")) return false;

    VkApplicationInfo appInfo{VK_STRUCTURE_TYPE_APPLICATION_INFO};
    appInfo.pApplicationName   = "CartridgeXR";
    appInfo.applicationVersion = VK_MAKE_VERSION(0, 0, 1);
    appInfo.pEngineName        = "CartridgeXR";
    appInfo.engineVersion      = VK_MAKE_VERSION(0, 0, 1);
    appInfo.apiVersion         = VK_API_VERSION_1_3;

    VkInstanceCreateInfo vkInstCreate{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
    vkInstCreate.pApplicationInfo = &appInfo;

    XrVulkanInstanceCreateInfoKHR xrInstCreate{XR_TYPE_VULKAN_INSTANCE_CREATE_INFO_KHR};
    xrInstCreate.systemId               = m_systemId;
    xrInstCreate.pfnGetInstanceProcAddr = &vkGetInstanceProcAddr;
    xrInstCreate.vulkanCreateInfo       = &vkInstCreate;
    xrInstCreate.vulkanAllocator        = nullptr;

    VkResult vkResult = VK_SUCCESS;
    if (!xr_check(pfnCreateVkInst(m_instance, &xrInstCreate, &m_vkInstance, &vkResult),
                  "xrCreateVulkanInstanceKHR")) return false;
    if (vkResult != VK_SUCCESS) {
        spdlog::error("[XrSession] vkCreateInstance via OpenXR failed: VkResult={}", static_cast<int>(vkResult));
        return false;
    }

    XrVulkanGraphicsDeviceGetInfoKHR getDevInfo{XR_TYPE_VULKAN_GRAPHICS_DEVICE_GET_INFO_KHR};
    getDevInfo.systemId       = m_systemId;
    getDevInfo.vulkanInstance = m_vkInstance;
    if (!xr_check(pfnGetVkPhys(m_instance, &getDevInfo, &m_vkPhysicalDevice),
                  "xrGetVulkanGraphicsDevice2KHR")) return false;

    // Pick a queue family that supports graphics.
    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(m_vkPhysicalDevice, &queueFamilyCount, nullptr);
    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(m_vkPhysicalDevice, &queueFamilyCount, queueFamilies.data());
    for (uint32_t i = 0; i < queueFamilyCount; ++i) {
        if (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            m_vkQueueFamily = i;
            break;
        }
    }
    if (m_vkQueueFamily == UINT32_MAX) {
        spdlog::error("[XrSession] no Vulkan graphics queue family found");
        return false;
    }

    const float queuePriority = 1.0f;
    VkDeviceQueueCreateInfo queueCreate{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
    queueCreate.queueFamilyIndex = m_vkQueueFamily;
    queueCreate.queueCount       = 1;
    queueCreate.pQueuePriorities = &queuePriority;

    VkPhysicalDeviceFeatures2 features2{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2};
    VkPhysicalDeviceMultiviewFeatures multiview{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MULTIVIEW_FEATURES};
    multiview.multiview = VK_TRUE;
    features2.pNext     = &multiview;

    VkDeviceCreateInfo vkDevCreate{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
    vkDevCreate.pNext                = &features2;
    vkDevCreate.queueCreateInfoCount = 1;
    vkDevCreate.pQueueCreateInfos    = &queueCreate;

    XrVulkanDeviceCreateInfoKHR xrDevCreate{XR_TYPE_VULKAN_DEVICE_CREATE_INFO_KHR};
    xrDevCreate.systemId               = m_systemId;
    xrDevCreate.pfnGetInstanceProcAddr = &vkGetInstanceProcAddr;
    xrDevCreate.vulkanPhysicalDevice   = m_vkPhysicalDevice;
    xrDevCreate.vulkanCreateInfo       = &vkDevCreate;
    xrDevCreate.vulkanAllocator        = nullptr;

    if (!xr_check(pfnCreateVkDev(m_instance, &xrDevCreate, &m_vkDevice, &vkResult),
                  "xrCreateVulkanDeviceKHR")) return false;
    if (vkResult != VK_SUCCESS) {
        spdlog::error("[XrSession] vkCreateDevice via OpenXR failed: VkResult={}", static_cast<int>(vkResult));
        return false;
    }

    vkGetDeviceQueue(m_vkDevice, m_vkQueueFamily, 0, &m_vkQueue);

    VkCommandPoolCreateInfo poolCreate{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
    poolCreate.flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    poolCreate.queueFamilyIndex = m_vkQueueFamily;
    if (vkCreateCommandPool(m_vkDevice, &poolCreate, nullptr, &m_vkCommandPool) != VK_SUCCESS) {
        spdlog::error("[XrSession] vkCreateCommandPool failed");
        return false;
    }

    spdlog::info("[XrSession] Vulkan device ready (queueFamily={})", m_vkQueueFamily);
    return true;
}

bool XrSession::createXrSession() {
    XrGraphicsBindingVulkan2KHR binding{XR_TYPE_GRAPHICS_BINDING_VULKAN2_KHR};
    binding.instance         = m_vkInstance;
    binding.physicalDevice   = m_vkPhysicalDevice;
    binding.device           = m_vkDevice;
    binding.queueFamilyIndex = m_vkQueueFamily;
    binding.queueIndex       = 0;

    XrSessionCreateInfo info{XR_TYPE_SESSION_CREATE_INFO};
    info.next     = &binding;
    info.systemId = m_systemId;
    if (!xr_check(xrCreateSession(m_instance, &info, &m_session), "xrCreateSession")) return false;

    XrReferenceSpaceCreateInfo spaceInfo{XR_TYPE_REFERENCE_SPACE_CREATE_INFO};
    spaceInfo.referenceSpaceType   = XR_REFERENCE_SPACE_TYPE_LOCAL;
    spaceInfo.poseInReferenceSpace = XrPosef{ {0,0,0,1}, {0,0,0} };
    if (!xr_check(xrCreateReferenceSpace(m_session, &spaceInfo, &m_appSpace),
                  "xrCreateReferenceSpace")) return false;

    return true;
}

bool XrSession::createSwapchains() {
    uint32_t formatCount = 0;
    if (!xr_check(xrEnumerateSwapchainFormats(m_session, 0, &formatCount, nullptr),
                  "xrEnumerateSwapchainFormats (count)")) return false;
    std::vector<int64_t> formats(formatCount);
    if (!xr_check(xrEnumerateSwapchainFormats(m_session, formatCount, &formatCount, formats.data()),
                  "xrEnumerateSwapchainFormats (data)")) return false;

    // Prefer sRGB B8G8R8A8; fall back to first supported.
    int64_t chosenFormat = formats.empty() ? 0 : formats.front();
    for (auto f : formats) {
        if (f == VK_FORMAT_B8G8R8A8_SRGB || f == VK_FORMAT_R8G8B8A8_SRGB) { chosenFormat = f; break; }
    }

    m_swapchains.reserve(m_viewConfigs.size());
    for (size_t i = 0; i < m_viewConfigs.size(); ++i) {
        const auto& v = m_viewConfigs[i];
        EyeSwapchain eye;
        eye.width  = static_cast<int32_t>(v.recommendedImageRectWidth);
        eye.height = static_cast<int32_t>(v.recommendedImageRectHeight);
        eye.format = chosenFormat;

        XrSwapchainCreateInfo info{XR_TYPE_SWAPCHAIN_CREATE_INFO};
        info.usageFlags  = XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_SAMPLED_BIT
                         | XR_SWAPCHAIN_USAGE_TRANSFER_DST_BIT;
        info.format      = chosenFormat;
        info.sampleCount = 1;
        info.width       = static_cast<uint32_t>(eye.width);
        info.height      = static_cast<uint32_t>(eye.height);
        info.faceCount   = 1;
        info.arraySize   = 1;
        info.mipCount    = 1;

        if (!xr_check(xrCreateSwapchain(m_session, &info, &eye.swapchain), "xrCreateSwapchain")) return false;

        uint32_t imageCount = 0;
        if (!xr_check(xrEnumerateSwapchainImages(eye.swapchain, 0, &imageCount, nullptr),
                      "xrEnumerateSwapchainImages (count)")) return false;
        eye.images.assign(imageCount, {XR_TYPE_SWAPCHAIN_IMAGE_VULKAN_KHR});
        if (!xr_check(xrEnumerateSwapchainImages(eye.swapchain, imageCount, &imageCount,
                                                 reinterpret_cast<XrSwapchainImageBaseHeader*>(eye.images.data())),
                      "xrEnumerateSwapchainImages (data)")) return false;

        spdlog::info("[XrSession] eye {} swapchain ready ({}x{}, format={}, images={})",
                     i, eye.width, eye.height, eye.format, imageCount);
        m_swapchains.push_back(std::move(eye));
    }

    return true;
}

void XrSession::pollEvents(bool& exitRequested) {
    XrEventDataBuffer ev{XR_TYPE_EVENT_DATA_BUFFER};
    while (xrPollEvent(m_instance, &ev) == XR_SUCCESS) {
        if (ev.type == XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED) {
            const auto* s = reinterpret_cast<XrEventDataSessionStateChanged*>(&ev);
            m_sessionState = s->state;
            spdlog::debug("[XrSession] state -> {}", static_cast<int>(s->state));
            if (s->state == XR_SESSION_STATE_READY) {
                XrSessionBeginInfo begin{XR_TYPE_SESSION_BEGIN_INFO};
                begin.primaryViewConfigurationType = kViewConfig;
                if (xr_check(xrBeginSession(m_session, &begin), "xrBeginSession")) {
                    m_sessionRunning = true;
                }
            } else if (s->state == XR_SESSION_STATE_STOPPING) {
                xr_check(xrEndSession(m_session), "xrEndSession");
                m_sessionRunning = false;
            } else if (s->state == XR_SESSION_STATE_EXITING || s->state == XR_SESSION_STATE_LOSS_PENDING) {
                exitRequested = true;
            }
        } else if (ev.type == XR_TYPE_EVENT_DATA_INSTANCE_LOSS_PENDING) {
            exitRequested = true;
        }
        ev = {XR_TYPE_EVENT_DATA_BUFFER};
    }
}

bool XrSession::pumpFrame() {
    bool exitRequested = false;
    pollEvents(exitRequested);
    if (exitRequested) return false;
    if (!m_sessionRunning) return true;

    XrFrameState frameState{XR_TYPE_FRAME_STATE};
    XrFrameWaitInfo waitInfo{XR_TYPE_FRAME_WAIT_INFO};
    if (!xr_check(xrWaitFrame(m_session, &waitInfo, &frameState), "xrWaitFrame")) return false;

    XrFrameBeginInfo beginInfo{XR_TYPE_FRAME_BEGIN_INFO};
    if (!xr_check(xrBeginFrame(m_session, &beginInfo), "xrBeginFrame")) return false;

    std::vector<XrCompositionLayerProjectionView> projViews(m_viewConfigs.size(),
        XrCompositionLayerProjectionView{XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW});

    if (frameState.shouldRender) {
        XrViewState         viewState{XR_TYPE_VIEW_STATE};
        XrViewLocateInfo    locate{XR_TYPE_VIEW_LOCATE_INFO};
        locate.viewConfigurationType = kViewConfig;
        locate.displayTime           = frameState.predictedDisplayTime;
        locate.space                 = m_appSpace;

        std::vector<XrView> views(m_viewConfigs.size(), {XR_TYPE_VIEW});
        uint32_t viewsOut = 0;
        if (xr_check(xrLocateViews(m_session, &locate, &viewState,
                                   static_cast<uint32_t>(views.size()), &viewsOut, views.data()),
                     "xrLocateViews")) {
            for (size_t i = 0; i < m_swapchains.size(); ++i) {
                auto& sc = m_swapchains[i];

                uint32_t imageIndex = 0;
                XrSwapchainImageAcquireInfo acquire{XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO};
                if (!xr_check(xrAcquireSwapchainImage(sc.swapchain, &acquire, &imageIndex),
                              "xrAcquireSwapchainImage")) continue;
                XrSwapchainImageWaitInfo waitImg{XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO};
                waitImg.timeout = XR_INFINITE_DURATION;
                if (!xr_check(xrWaitSwapchainImage(sc.swapchain, &waitImg), "xrWaitSwapchainImage")) continue;

                // Magenta clear.
                VkCommandBufferAllocateInfo allocInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
                allocInfo.commandPool        = m_vkCommandPool;
                allocInfo.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
                allocInfo.commandBufferCount = 1;
                VkCommandBuffer cmd = VK_NULL_HANDLE;
                vkAllocateCommandBuffers(m_vkDevice, &allocInfo, &cmd);

                VkCommandBufferBeginInfo bi{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
                bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
                vkBeginCommandBuffer(cmd, &bi);

                clearImageMagenta(cmd, sc.images[imageIndex].image,
                                  static_cast<uint32_t>(sc.width),
                                  static_cast<uint32_t>(sc.height),
                                  VK_IMAGE_LAYOUT_UNDEFINED,
                                  VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

                vkEndCommandBuffer(cmd);

                VkSubmitInfo submit{VK_STRUCTURE_TYPE_SUBMIT_INFO};
                submit.commandBufferCount = 1;
                submit.pCommandBuffers    = &cmd;
                vkQueueSubmit(m_vkQueue, 1, &submit, VK_NULL_HANDLE);
                vkQueueWaitIdle(m_vkQueue);
                vkFreeCommandBuffers(m_vkDevice, m_vkCommandPool, 1, &cmd);

                XrSwapchainImageReleaseInfo release{XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO};
                xrReleaseSwapchainImage(sc.swapchain, &release);

                projViews[i].pose                          = views[i].pose;
                projViews[i].fov                           = views[i].fov;
                projViews[i].subImage.swapchain            = sc.swapchain;
                projViews[i].subImage.imageRect.offset     = {0, 0};
                projViews[i].subImage.imageRect.extent     = {sc.width, sc.height};
                projViews[i].subImage.imageArrayIndex      = 0;
            }
        }
    }

    XrCompositionLayerProjection projLayer{XR_TYPE_COMPOSITION_LAYER_PROJECTION};
    projLayer.space     = m_appSpace;
    projLayer.viewCount = static_cast<uint32_t>(projViews.size());
    projLayer.views     = projViews.data();

    std::array<const XrCompositionLayerBaseHeader*, 1> layers{
        reinterpret_cast<const XrCompositionLayerBaseHeader*>(&projLayer)
    };

    XrFrameEndInfo end{XR_TYPE_FRAME_END_INFO};
    end.displayTime          = frameState.predictedDisplayTime;
    end.environmentBlendMode = kBlendMode;
    end.layerCount           = frameState.shouldRender ? 1u : 0u;
    end.layers               = frameState.shouldRender ? layers.data() : nullptr;
    return xr_check(xrEndFrame(m_session, &end), "xrEndFrame");
}

void XrSession::clearImageMagenta(VkCommandBuffer cmd, VkImage image,
                                  uint32_t /*width*/, uint32_t /*height*/,
                                  VkImageLayout initialLayout, VkImageLayout finalLayout) {
    auto transition = [&](VkImageLayout from, VkImageLayout to,
                          VkAccessFlags srcAccess, VkAccessFlags dstAccess,
                          VkPipelineStageFlags srcStage, VkPipelineStageFlags dstStage) {
        VkImageMemoryBarrier b{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
        b.oldLayout                       = from;
        b.newLayout                       = to;
        b.srcAccessMask                   = srcAccess;
        b.dstAccessMask                   = dstAccess;
        b.srcQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
        b.dstQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
        b.image                           = image;
        b.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
        b.subresourceRange.baseMipLevel   = 0;
        b.subresourceRange.levelCount     = 1;
        b.subresourceRange.baseArrayLayer = 0;
        b.subresourceRange.layerCount     = 1;
        vkCmdPipelineBarrier(cmd, srcStage, dstStage, 0, 0, nullptr, 0, nullptr, 1, &b);
    };

    transition(initialLayout, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
               0, VK_ACCESS_TRANSFER_WRITE_BIT,
               VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);

    VkClearColorValue magenta{};
    magenta.float32[0] = 1.0f;
    magenta.float32[1] = 0.0f;
    magenta.float32[2] = 1.0f;
    magenta.float32[3] = 1.0f;
    VkImageSubresourceRange range{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    vkCmdClearColorImage(cmd, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &magenta, 1, &range);

    transition(VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, finalLayout,
               VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
               VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
}

void XrSession::shutdown() {
    for (auto& sc : m_swapchains) {
        if (sc.swapchain != XR_NULL_HANDLE) xrDestroySwapchain(sc.swapchain);
    }
    m_swapchains.clear();

    if (m_appSpace != XR_NULL_HANDLE)  { xrDestroySpace(m_appSpace);  m_appSpace = XR_NULL_HANDLE; }
    if (m_session  != XR_NULL_HANDLE)  { xrDestroySession(m_session); m_session  = XR_NULL_HANDLE; }
    if (m_instance != XR_NULL_HANDLE)  { xrDestroyInstance(m_instance); m_instance = XR_NULL_HANDLE; }

    if (m_vkCommandPool != VK_NULL_HANDLE) { vkDestroyCommandPool(m_vkDevice, m_vkCommandPool, nullptr); m_vkCommandPool = VK_NULL_HANDLE; }
    if (m_vkDevice      != VK_NULL_HANDLE) { vkDestroyDevice(m_vkDevice, nullptr);                       m_vkDevice      = VK_NULL_HANDLE; }
    if (m_vkInstance    != VK_NULL_HANDLE) { vkDestroyInstance(m_vkInstance, nullptr);                   m_vkInstance    = VK_NULL_HANDLE; }
}

}  // namespace cxr
