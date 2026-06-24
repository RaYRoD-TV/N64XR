// ============================================================================
//  UiHost.cpp — GLFW + Vulkan + ImGui boot/teardown + main loop.
// ----------------------------------------------------------------------------
//  Architecture notes:
//    * The launcher creates its OWN VkInstance + VkDevice. It picks the
//      same VkPhysicalDevice OpenXR will end up picking, but does NOT try
//      to share a logical device with the headset session — that path
//      forces xrCreateVulkanDeviceKHR to be the device factory, which we
//      don't want for a shell that has to run with no headset attached.
//      In-VR rendering (Workstream G) will own its own device, sample
//      ImGui's output as a VkImage, and present via the XR compositor.
//    * Descriptor pool is oversized (1000 of each common type) per the
//      vkguide.dev recommendation — ImGui re-allocates from it on demand.
//    * vkDeviceWaitIdle is called before EVERY destructor — driver
//      validation will scream otherwise.
// ============================================================================

#include "UiHost.h"

#include "Theme.h"
#include "AliveIdle.h"
#include "Screens.h"
#include "AppState.h"
#include "HoloStage.h"

// GLFW_INCLUDE_VULKAN is set as a frontend compile def (see src/frontend/CMakeLists.txt)
// so glfw3.h pulls in vulkan.h automatically — don't redefine here.
#include <GLFW/glfw3.h>

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>

#include <spdlog/spdlog.h>

#include <algorithm>
#include <array>
#include <cstring>
#include <optional>
#include <set>
#include <stdexcept>
#include <vector>

// ---------------------------------------------------------------------------
//  Helpers
// ---------------------------------------------------------------------------
namespace {

constexpr uint32_t kInitialWidth  = 1600;
constexpr uint32_t kInitialHeight = 980;

void VkCheck(VkResult r, const char* what) {
    if (r != VK_SUCCESS) {
        throw std::runtime_error(std::string("Vulkan: ") + what +
                                 " failed (VkResult=" + std::to_string(int(r)) + ")");
    }
}

struct QueueFamilies {
    std::optional<uint32_t> graphicsAndPresent;
    bool complete() const { return graphicsAndPresent.has_value(); }
};

QueueFamilies PickQueueFamilies(VkPhysicalDevice phys, VkSurfaceKHR surface) {
    uint32_t count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(phys, &count, nullptr);
    std::vector<VkQueueFamilyProperties> props(count);
    vkGetPhysicalDeviceQueueFamilyProperties(phys, &count, props.data());

    QueueFamilies out{};
    for (uint32_t i = 0; i < count; ++i) {
        const bool hasGfx = (props[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0;
        VkBool32 hasPresent = VK_FALSE;
        vkGetPhysicalDeviceSurfaceSupportKHR(phys, i, surface, &hasPresent);
        if (hasGfx && hasPresent) {
            out.graphicsAndPresent = i;
            break;
        }
    }
    return out;
}

VkPhysicalDevice PickPhysical(VkInstance inst, VkSurfaceKHR surface,
                              QueueFamilies& outFamilies) {
    uint32_t count = 0;
    vkEnumeratePhysicalDevices(inst, &count, nullptr);
    if (count == 0) {
        throw std::runtime_error("No Vulkan-capable GPU found.");
    }
    std::vector<VkPhysicalDevice> devices(count);
    vkEnumeratePhysicalDevices(inst, &count, devices.data());

    // Prefer discrete; the 5090 will be first on Ray's rig.
    std::sort(devices.begin(), devices.end(),
              [](VkPhysicalDevice a, VkPhysicalDevice b) {
        VkPhysicalDeviceProperties pa{}, pb{};
        vkGetPhysicalDeviceProperties(a, &pa);
        vkGetPhysicalDeviceProperties(b, &pb);
        auto rank = [](VkPhysicalDeviceType t) {
            switch (t) {
                case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:   return 4;
                case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU: return 3;
                case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:    return 2;
                case VK_PHYSICAL_DEVICE_TYPE_CPU:            return 1;
                default:                                     return 0;
            }
        };
        return rank(pa.deviceType) > rank(pb.deviceType);
    });

    for (auto d : devices) {
        auto qf = PickQueueFamilies(d, surface);
        if (qf.complete()) {
            VkPhysicalDeviceProperties p{};
            vkGetPhysicalDeviceProperties(d, &p);
            spdlog::info("GPU selected: {} (Vulkan {}.{}.{})",
                         p.deviceName,
                         VK_VERSION_MAJOR(p.apiVersion),
                         VK_VERSION_MINOR(p.apiVersion),
                         VK_VERSION_PATCH(p.apiVersion));
            outFamilies = qf;
            return d;
        }
    }
    throw std::runtime_error("No GPU with a graphics+present queue.");
}

} // unnamed namespace

// ---------------------------------------------------------------------------
//  Pimpl
// ---------------------------------------------------------------------------
namespace n64xr::ui {

struct UiHost::Impl {
    GLFWwindow*               window     = nullptr;

    VkInstance                instance   = VK_NULL_HANDLE;
    VkSurfaceKHR              surface    = VK_NULL_HANDLE;
    VkPhysicalDevice          physical   = VK_NULL_HANDLE;
    VkDevice                  device     = VK_NULL_HANDLE;
    uint32_t                  gfxFamily  = 0;
    VkQueue                   gfxQueue   = VK_NULL_HANDLE;

    VkSwapchainKHR            swapchain  = VK_NULL_HANDLE;
    VkFormat                  scFormat   = VK_FORMAT_B8G8R8A8_UNORM;
    VkExtent2D                scExtent   = {kInitialWidth, kInitialHeight};
    std::vector<VkImage>      scImages;
    std::vector<VkImageView>  scViews;
    std::vector<VkFramebuffer>scFbs;
    VkRenderPass              renderPass = VK_NULL_HANDLE;

    VkCommandPool             cmdPool    = VK_NULL_HANDLE;
    std::vector<VkCommandBuffer> cmdBuffers;

    static constexpr int kFramesInFlight = 2;
    std::array<VkSemaphore, kFramesInFlight> semImageAvail{};
    std::array<VkSemaphore, kFramesInFlight> semRenderDone{};
    std::array<VkFence,     kFramesInFlight> inFlight{};
    uint32_t frameIdx = 0;

    VkDescriptorPool          imguiPool  = VK_NULL_HANDLE;

    n64xr::holo::HoloStage    holo;
    bool                      holoReady = false;

    alive::State              aliveState;
    bool                      swapchainDirty = false;

    // ---- lifecycle ----
    void initGlfw();
    void initVulkan();
    void createSwapchain();
    void destroySwapchain();
    void recreateSwapchain();
    void initImGui();
    void mainLoop(UiHost& outer);
    void drawFrame(UiHost& outer);
    void shutdown();

    // ---- ui composition ----
    void composeFrame(UiHost& outer);
};

UiHost::UiHost()  : m_impl(new Impl()) {}
UiHost::~UiHost() { if (m_impl) { m_impl->shutdown(); delete m_impl; } }

int UiHost::run(int /*argc*/, char** /*argv*/) {
    m_impl->initGlfw();
    m_impl->initVulkan();
    m_impl->createSwapchain();
    m_impl->initImGui();

    m_impl->holo.init(
        m_impl->physical, m_impl->device, m_impl->gfxFamily,
        m_impl->gfxQueue, m_impl->cmdPool, m_impl->scExtent,
        m_impl->scFormat, m_impl->renderPass, "assets/shaders");
    m_impl->holoReady = true;

    // Friendly first status line.
    m_state.statusLine    = "Awaiting cartridge.";
    m_state.openxrRuntime = "OpenXR runtime: not yet probed.";

    m_impl->mainLoop(*this);
    return 0;
}

// ---------------------------------------------------------------------------
//  GLFW
// ---------------------------------------------------------------------------
void UiHost::Impl::initGlfw() {
    glfwSetErrorCallback([](int code, const char* desc){
        spdlog::error("GLFW {}: {}", code, desc);
    });
    if (!glfwInit()) throw std::runtime_error("glfwInit failed");
    if (!glfwVulkanSupported())
        throw std::runtime_error("GLFW reports Vulkan is not supported on this system.");

    glfwWindowHint(GLFW_CLIENT_API,       GLFW_NO_API);
    glfwWindowHint(GLFW_SCALE_TO_MONITOR, GLFW_TRUE);
    glfwWindowHint(GLFW_RESIZABLE,        GLFW_TRUE);

    window = glfwCreateWindow(static_cast<int>(kInitialWidth),
                              static_cast<int>(kInitialHeight),
                              "N64XR  -  ready to slot a cartridge",
                              nullptr, nullptr);
    if (!window) throw std::runtime_error("glfwCreateWindow failed");

    glfwSetWindowUserPointer(window, this);
    glfwSetFramebufferSizeCallback(window, [](GLFWwindow* w, int, int){
        auto* self = static_cast<Impl*>(glfwGetWindowUserPointer(w));
        self->swapchainDirty = true;
    });
}

// ---------------------------------------------------------------------------
//  Vulkan instance + device
// ---------------------------------------------------------------------------
void UiHost::Impl::initVulkan() {
    // ---- Instance -------------------------------------------------------
    VkApplicationInfo app{ VK_STRUCTURE_TYPE_APPLICATION_INFO };
    app.pApplicationName   = "N64XR Launcher";
    app.applicationVersion = VK_MAKE_VERSION(0, 1, 0);
    app.pEngineName        = "N64XR";
    app.engineVersion      = VK_MAKE_VERSION(0, 1, 0);
    app.apiVersion         = VK_API_VERSION_1_3;

    uint32_t glfwExtCount = 0;
    const char** glfwExts = glfwGetRequiredInstanceExtensions(&glfwExtCount);
    std::vector<const char*> exts(glfwExts, glfwExts + glfwExtCount);

    VkInstanceCreateInfo ici{ VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO };
    ici.pApplicationInfo        = &app;
    ici.enabledExtensionCount   = static_cast<uint32_t>(exts.size());
    ici.ppEnabledExtensionNames = exts.data();
    VkCheck(vkCreateInstance(&ici, nullptr, &instance), "vkCreateInstance");

    // ---- Surface --------------------------------------------------------
    VkCheck(glfwCreateWindowSurface(instance, window, nullptr, &surface),
            "glfwCreateWindowSurface");

    // ---- Physical + logical device -------------------------------------
    QueueFamilies qf{};
    physical  = PickPhysical(instance, surface, qf);
    gfxFamily = *qf.graphicsAndPresent;

    float prio = 1.0f;
    VkDeviceQueueCreateInfo qci{ VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO };
    qci.queueFamilyIndex = gfxFamily;
    qci.queueCount       = 1;
    qci.pQueuePriorities = &prio;

    std::array<const char*, 1> devExts{ VK_KHR_SWAPCHAIN_EXTENSION_NAME };

    VkPhysicalDeviceFeatures feats{};
    feats.samplerAnisotropy = VK_TRUE;

    VkDeviceCreateInfo dci{ VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO };
    dci.queueCreateInfoCount    = 1;
    dci.pQueueCreateInfos       = &qci;
    dci.enabledExtensionCount   = static_cast<uint32_t>(devExts.size());
    dci.ppEnabledExtensionNames = devExts.data();
    dci.pEnabledFeatures        = &feats;
    VkCheck(vkCreateDevice(physical, &dci, nullptr, &device), "vkCreateDevice");
    vkGetDeviceQueue(device, gfxFamily, 0, &gfxQueue);

    // ---- Command pool ---------------------------------------------------
    VkCommandPoolCreateInfo pci{ VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO };
    pci.flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    pci.queueFamilyIndex = gfxFamily;
    VkCheck(vkCreateCommandPool(device, &pci, nullptr, &cmdPool), "vkCreateCommandPool");

    // ---- Sync -----------------------------------------------------------
    VkSemaphoreCreateInfo sci{ VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
    VkFenceCreateInfo     fci{ VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
    fci.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    for (int i = 0; i < kFramesInFlight; ++i) {
        VkCheck(vkCreateSemaphore(device, &sci, nullptr, &semImageAvail[i]), "sem A");
        VkCheck(vkCreateSemaphore(device, &sci, nullptr, &semRenderDone[i]), "sem R");
        VkCheck(vkCreateFence    (device, &fci, nullptr, &inFlight[i]),      "fence");
    }
}

// ---------------------------------------------------------------------------
//  Swapchain
// ---------------------------------------------------------------------------
void UiHost::Impl::createSwapchain() {
    VkSurfaceCapabilitiesKHR caps{};
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physical, surface, &caps);

    int fbw, fbh;
    glfwGetFramebufferSize(window, &fbw, &fbh);
    scExtent.width  = std::clamp<uint32_t>(static_cast<uint32_t>(fbw),
                                           caps.minImageExtent.width,
                                           caps.maxImageExtent.width);
    scExtent.height = std::clamp<uint32_t>(static_cast<uint32_t>(fbh),
                                           caps.minImageExtent.height,
                                           caps.maxImageExtent.height);

    // Pick a sensible format: BGRA8 UNORM is universal; ImGui's default
    // backend works in linear-byte space, so don't go SRGB here.
    uint32_t fmtCount = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(physical, surface, &fmtCount, nullptr);
    std::vector<VkSurfaceFormatKHR> formats(fmtCount);
    vkGetPhysicalDeviceSurfaceFormatsKHR(physical, surface, &fmtCount, formats.data());
    VkSurfaceFormatKHR chosen = formats[0];
    for (auto& f : formats) {
        if (f.format == VK_FORMAT_B8G8R8A8_UNORM &&
            f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            chosen = f; break;
        }
    }
    scFormat = chosen.format;

    uint32_t imgCount = caps.minImageCount + 1;
    if (caps.maxImageCount > 0 && imgCount > caps.maxImageCount)
        imgCount = caps.maxImageCount;

    VkSwapchainCreateInfoKHR sci{ VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR };
    sci.surface          = surface;
    sci.minImageCount    = imgCount;
    sci.imageFormat      = chosen.format;
    sci.imageColorSpace  = chosen.colorSpace;
    sci.imageExtent      = scExtent;
    sci.imageArrayLayers = 1;
    sci.imageUsage       = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    sci.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    sci.preTransform     = caps.currentTransform;
    sci.compositeAlpha   = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    sci.presentMode      = VK_PRESENT_MODE_FIFO_KHR; // vsynced, dead simple
    sci.clipped          = VK_TRUE;
    VkCheck(vkCreateSwapchainKHR(device, &sci, nullptr, &swapchain), "vkCreateSwapchainKHR");

    uint32_t actual = 0;
    vkGetSwapchainImagesKHR(device, swapchain, &actual, nullptr);
    scImages.resize(actual);
    vkGetSwapchainImagesKHR(device, swapchain, &actual, scImages.data());

    // ---- Render pass ----------------------------------------------------
    VkAttachmentDescription colour{};
    colour.format         = scFormat;
    colour.samples        = VK_SAMPLE_COUNT_1_BIT;
    colour.loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colour.storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
    colour.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colour.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colour.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
    colour.finalLayout    = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference ref{0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
    VkSubpassDescription sub{};
    sub.pipelineBindPoint    = VK_PIPELINE_BIND_POINT_GRAPHICS;
    sub.colorAttachmentCount = 1;
    sub.pColorAttachments    = &ref;

    VkSubpassDependency dep{};
    dep.srcSubpass    = VK_SUBPASS_EXTERNAL;
    dep.dstSubpass    = 0;
    dep.srcStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dep.dstStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dep.srcAccessMask = 0;
    dep.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    VkRenderPassCreateInfo rpci{ VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO };
    rpci.attachmentCount = 1;
    rpci.pAttachments    = &colour;
    rpci.subpassCount    = 1;
    rpci.pSubpasses      = &sub;
    rpci.dependencyCount = 1;
    rpci.pDependencies   = &dep;
    VkCheck(vkCreateRenderPass(device, &rpci, nullptr, &renderPass), "vkCreateRenderPass");

    // ---- Views + framebuffers ------------------------------------------
    scViews.resize(scImages.size());
    scFbs  .resize(scImages.size());
    for (size_t i = 0; i < scImages.size(); ++i) {
        VkImageViewCreateInfo ivci{ VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
        ivci.image    = scImages[i];
        ivci.viewType = VK_IMAGE_VIEW_TYPE_2D;
        ivci.format   = scFormat;
        ivci.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
        VkCheck(vkCreateImageView(device, &ivci, nullptr, &scViews[i]), "vkCreateImageView");

        VkFramebufferCreateInfo fbci{ VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO };
        fbci.renderPass      = renderPass;
        fbci.attachmentCount = 1;
        fbci.pAttachments    = &scViews[i];
        fbci.width           = scExtent.width;
        fbci.height          = scExtent.height;
        fbci.layers          = 1;
        VkCheck(vkCreateFramebuffer(device, &fbci, nullptr, &scFbs[i]), "vkCreateFramebuffer");
    }

    // ---- Command buffers -----------------------------------------------
    cmdBuffers.resize(scImages.size());
    VkCommandBufferAllocateInfo cbai{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
    cbai.commandPool        = cmdPool;
    cbai.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cbai.commandBufferCount = static_cast<uint32_t>(cmdBuffers.size());
    VkCheck(vkAllocateCommandBuffers(device, &cbai, cmdBuffers.data()),
            "vkAllocateCommandBuffers");
}

void UiHost::Impl::destroySwapchain() {
    if (!device) return;
    vkDeviceWaitIdle(device);
    if (!cmdBuffers.empty()) {
        vkFreeCommandBuffers(device, cmdPool,
                             static_cast<uint32_t>(cmdBuffers.size()),
                             cmdBuffers.data());
        cmdBuffers.clear();
    }
    for (auto fb : scFbs)   if (fb) vkDestroyFramebuffer(device, fb, nullptr);
    for (auto v  : scViews) if (v ) vkDestroyImageView (device, v , nullptr);
    scFbs.clear(); scViews.clear();
    if (renderPass) { vkDestroyRenderPass(device, renderPass, nullptr); renderPass = VK_NULL_HANDLE; }
    if (swapchain)  { vkDestroySwapchainKHR(device, swapchain, nullptr); swapchain  = VK_NULL_HANDLE; }
}

void UiHost::Impl::recreateSwapchain() {
    int w = 0, h = 0;
    glfwGetFramebufferSize(window, &w, &h);
    while (w == 0 || h == 0) {  // minimised
        glfwWaitEvents();
        glfwGetFramebufferSize(window, &w, &h);
    }
    destroySwapchain();
    createSwapchain();
    if (imguiPool) {
        ImGui_ImplVulkan_SetMinImageCount(2);
    }
    if (holoReady) holo.resize(scExtent);
    swapchainDirty = false;
}

// ---------------------------------------------------------------------------
//  ImGui
// ---------------------------------------------------------------------------
void UiHost::Impl::initImGui() {
    // ---- Descriptor pool ------------------------------------------------
    const VkDescriptorPoolSize sizes[] = {
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
        { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,           1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,          1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,          1000 },
    };
    VkDescriptorPoolCreateInfo dpci{ VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
    dpci.flags         = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    dpci.maxSets       = 1000 * IM_ARRAYSIZE(sizes);
    dpci.poolSizeCount = IM_ARRAYSIZE(sizes);
    dpci.pPoolSizes    = sizes;
    VkCheck(vkCreateDescriptorPool(device, &dpci, nullptr, &imguiPool),
            "vkCreateDescriptorPool (imgui)");

    // ---- ImGui core -----------------------------------------------------
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.IniFilename  = nullptr; // we don't want a window.ini polluting the launcher

    float xs = 1.0f, ys = 1.0f;
    glfwGetWindowContentScale(window, &xs, &ys);
    const float dpi = std::max(xs, 1.0f);

    theme::LoadFonts(dpi);
    theme::ApplyStyle(dpi);

    ImGui_ImplGlfw_InitForVulkan(window, true);
    ImGui_ImplVulkan_InitInfo vi{};
    vi.Instance       = instance;
    vi.PhysicalDevice = physical;
    vi.Device         = device;
    vi.QueueFamily    = gfxFamily;
    vi.Queue          = gfxQueue;
    vi.DescriptorPool = imguiPool;
    vi.MinImageCount  = 2;
    vi.ImageCount     = static_cast<uint32_t>(scImages.size());
    // ImGui 1.92 (2025-09-26): RenderPass / Subpass / MSAASamples moved into
    // ImGui_ImplVulkan_PipelineInfo PipelineInfoMain.
    vi.PipelineInfoMain.RenderPass  = renderPass;
    vi.PipelineInfoMain.Subpass     = 0;
    vi.PipelineInfoMain.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
    ImGui_ImplVulkan_Init(&vi);

    spdlog::info("ImGui initialised at {:.2f}x DPI scale ({} swapchain images).",
                 dpi, scImages.size());
}

// ---------------------------------------------------------------------------
//  Main loop
// ---------------------------------------------------------------------------
void UiHost::Impl::mainLoop(UiHost& outer) {
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        if (swapchainDirty) recreateSwapchain();
        drawFrame(outer);
    }
}

void UiHost::Impl::composeFrame(UiHost& outer) {
    // ---- Full-window root --------------------------------------------------
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos (vp->WorkPos);
    ImGui::SetNextWindowSize(vp->WorkSize);
    ImGui::SetNextWindowViewport(vp->ID);

    constexpr ImGuiWindowFlags rootFlags =
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove     | ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus |
        ImGuiWindowFlags_MenuBar    | ImGuiWindowFlags_NoScrollbar;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0,0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::Begin("##n64xr_root", nullptr, rootFlags);
    ImGui::PopStyleVar(3);

    // ---- Alive-idle bed (drawn UNDER everything) --------------------------
    if (outer.m_state.showAliveIdle) {
        ImDrawList* dl = ImGui::GetWindowDrawList();
        const ImVec2 mn = ImGui::GetWindowPos();
        const ImVec2 mx = ImVec2(mn.x + ImGui::GetWindowSize().x,
                                 mn.y + ImGui::GetWindowSize().y);
        alive::PaintBed(dl, mn, mx, outer.m_state.elapsedSeconds(),
                        aliveState, outer.m_state.showScanlines);
    }

    // ---- Menu bar ---------------------------------------------------------
    if (ImGui::BeginMenuBar()) {
        if (ImGui::BeginMenu("Console")) {
            if (ImGui::MenuItem("Step Into the Room")) {
                outer.m_state.currentScreen      = Screen::Home;
                outer.m_state.vrSessionInFlight  = true;
            }
            if (ImGui::MenuItem("Smoke Test (270 magenta frames)")) {
                outer.m_state.currentScreen      = Screen::Home;
                outer.m_state.vrSessionInFlight  = true;
                outer.m_state.lastSmokeTestNote  = "Smoke test queued from Console menu.";
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Power Down")) {
                glfwSetWindowShouldClose(window, GLFW_TRUE);
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Equipment")) {
            if (ImGui::MenuItem("Slot Cartridge...")) {
                outer.m_state.currentScreen = Screen::CartridgeVault;
            }
            if (ImGui::MenuItem("Bring Out the Toolbox")) {
                outer.m_state.currentScreen = Screen::ServiceHatch;
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("View")) {
            ImGui::MenuItem("Cold Light (dark theme)", nullptr, &outer.m_state.coldLight);
            ImGui::MenuItem("Alive Idle layer",        nullptr, &outer.m_state.showAliveIdle);
            ImGui::MenuItem("Scanlines",               nullptr, &outer.m_state.showScanlines);
            ImGui::EndMenu();
        }
        ImGui::EndMenuBar();
    }

    // ---- Tab spine (the XMB-inspired horizontal axis) ---------------------
    ImGui::Dummy(ImVec2(0, 6));
    ImGui::Indent(24.0f);
    auto& palette = theme::Colours();
    const ImVec4 tabIdle   = palette.agedPaperDim;
    const ImVec4 tabActive = palette.brassHot;

    auto tab = [&](const char* label, Screen s) {
        const bool active = (outer.m_state.currentScreen == s);
        ImGui::PushStyleColor(ImGuiCol_Text, active ? tabActive : tabIdle);
        ImGui::PushFont(theme::GetFonts().displaySmall ? theme::GetFonts().displaySmall : ImGui::GetFont());
        if (ImGui::Selectable(label, active, 0,
                              ImVec2(ImGui::CalcTextSize(label).x + 28.0f, 0))) {
            outer.m_state.currentScreen = s;
        }
        ImGui::PopFont();
        ImGui::PopStyleColor();
        if (active) {
            // brass underline
            ImDrawList* dl  = ImGui::GetWindowDrawList();
            const ImVec2 mn = ImGui::GetItemRectMin();
            const ImVec2 mx = ImGui::GetItemRectMax();
            const ImU32  col= ImGui::ColorConvertFloat4ToU32(tabActive);
            dl->AddLine(ImVec2(mn.x + 8, mx.y + 2), ImVec2(mx.x - 8, mx.y + 2), col, 2.0f);
        }
        ImGui::SameLine();
    };
    tab("  HOME  ",            Screen::Home);
    tab("  CARTRIDGE VAULT  ", Screen::CartridgeVault);
    tab("  SERVICE HATCH  ",   Screen::ServiceHatch);
    ImGui::NewLine();
    ImGui::Unindent(24.0f);

    // ---- Active screen ----------------------------------------------------
    ImGui::Dummy(ImVec2(0, 8));
    ImGui::Indent(24.0f);
    switch (outer.m_state.currentScreen) {
        case Screen::Home:           DrawHomeScreen       (outer.m_state); break;
        case Screen::CartridgeVault: DrawRomBrowserScreen (outer.m_state); break;
        case Screen::ServiceHatch:   DrawSettingsScreen   (outer.m_state); break;
    }
    ImGui::Unindent(24.0f);

    // ---- Phosphor strip ---------------------------------------------------
    DrawPhosphorStatusStrip(outer.m_state);

    ImGui::End();
}

void UiHost::Impl::drawFrame(UiHost& outer) {
    vkWaitForFences(device, 1, &inFlight[frameIdx], VK_TRUE, UINT64_MAX);

    uint32_t imageIdx = 0;
    VkResult acq = vkAcquireNextImageKHR(device, swapchain, UINT64_MAX,
                                         semImageAvail[frameIdx], VK_NULL_HANDLE,
                                         &imageIdx);
    if (acq == VK_ERROR_OUT_OF_DATE_KHR) { swapchainDirty = true; return; }
    if (acq != VK_SUCCESS && acq != VK_SUBOPTIMAL_KHR) {
        throw std::runtime_error("vkAcquireNextImageKHR failed");
    }
    vkResetFences(device, 1, &inFlight[frameIdx]);

    // ---- ImGui frame ----------------------------------------------------
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
    composeFrame(outer);
    ImGui::Render();
    ImDrawData* dd = ImGui::GetDrawData();

    // ---- Record command buffer -----------------------------------------
    VkCommandBuffer cmd = cmdBuffers[imageIdx];
    vkResetCommandBuffer(cmd, 0);
    VkCommandBufferBeginInfo bi{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
    bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    VkCheck(vkBeginCommandBuffer(cmd, &bi), "vkBeginCommandBuffer");

    // 3D holo cartridge + bloom into HoloStage's own offscreen targets —
    // MUST happen before the swapchain render pass begins.
    if (holoReady) {
        double mxp = 0.0, myp = 0.0;
        glfwGetCursorPos(window, &mxp, &myp);
        int ww = 0, wh = 0;
        glfwGetWindowSize(window, &ww, &wh);
        const float px = (ww > 0) ? float((mxp / double(ww)) * 2.0 - 1.0) : 0.0f;
        const float py = (wh > 0) ? float((myp / double(wh)) * 2.0 - 1.0) : 0.0f;
        holo.renderOffscreen(cmd, outer.m_state.elapsedSeconds(), px, py);
    }

    VkClearValue clear{};
    auto& bg = theme::Colours().deepNavyBg;
    clear.color = { { bg.x, bg.y, bg.z, bg.w } };

    VkRenderPassBeginInfo rp{ VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO };
    rp.renderPass        = renderPass;
    rp.framebuffer       = scFbs[imageIdx];
    rp.renderArea.offset = {0,0};
    rp.renderArea.extent = scExtent;
    rp.clearValueCount   = 1;
    rp.pClearValues      = &clear;
    vkCmdBeginRenderPass(cmd, &rp, VK_SUBPASS_CONTENTS_INLINE);
    if (holoReady) holo.compositeFullscreen(cmd, scExtent);
    ImGui_ImplVulkan_RenderDrawData(dd, cmd);
    vkCmdEndRenderPass(cmd);
    VkCheck(vkEndCommandBuffer(cmd), "vkEndCommandBuffer");

    // ---- Submit --------------------------------------------------------
    VkPipelineStageFlags wait = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    VkSubmitInfo si{ VK_STRUCTURE_TYPE_SUBMIT_INFO };
    si.waitSemaphoreCount   = 1;
    si.pWaitSemaphores      = &semImageAvail[frameIdx];
    si.pWaitDstStageMask    = &wait;
    si.commandBufferCount   = 1;
    si.pCommandBuffers      = &cmd;
    si.signalSemaphoreCount = 1;
    si.pSignalSemaphores    = &semRenderDone[frameIdx];
    VkCheck(vkQueueSubmit(gfxQueue, 1, &si, inFlight[frameIdx]), "vkQueueSubmit");

    VkPresentInfoKHR pi{ VK_STRUCTURE_TYPE_PRESENT_INFO_KHR };
    pi.waitSemaphoreCount = 1;
    pi.pWaitSemaphores    = &semRenderDone[frameIdx];
    pi.swapchainCount     = 1;
    pi.pSwapchains        = &swapchain;
    pi.pImageIndices      = &imageIdx;
    VkResult pres = vkQueuePresentKHR(gfxQueue, &pi);
    if (pres == VK_ERROR_OUT_OF_DATE_KHR || pres == VK_SUBOPTIMAL_KHR) {
        swapchainDirty = true;
    } else if (pres != VK_SUCCESS) {
        throw std::runtime_error("vkQueuePresentKHR failed");
    }

    frameIdx = (frameIdx + 1) % kFramesInFlight;
}

// ---------------------------------------------------------------------------
//  Shutdown — reverse order, drain GPU first.
// ---------------------------------------------------------------------------
void UiHost::Impl::shutdown() {
    if (device) vkDeviceWaitIdle(device);

    if (holoReady) { holo.shutdown(); holoReady = false; }

    if (ImGui::GetCurrentContext()) {
        ImGui_ImplVulkan_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
    }

    if (device && imguiPool) {
        vkDestroyDescriptorPool(device, imguiPool, nullptr);
        imguiPool = VK_NULL_HANDLE;
    }

    destroySwapchain();

    if (device) {
        for (int i = 0; i < kFramesInFlight; ++i) {
            if (semImageAvail[i]) vkDestroySemaphore(device, semImageAvail[i], nullptr);
            if (semRenderDone[i]) vkDestroySemaphore(device, semRenderDone[i], nullptr);
            if (inFlight[i])      vkDestroyFence    (device, inFlight[i],      nullptr);
            semImageAvail[i] = VK_NULL_HANDLE;
            semRenderDone[i] = VK_NULL_HANDLE;
            inFlight[i]      = VK_NULL_HANDLE;
        }
        if (cmdPool) { vkDestroyCommandPool(device, cmdPool, nullptr); cmdPool = VK_NULL_HANDLE; }
        vkDestroyDevice(device, nullptr);
        device = VK_NULL_HANDLE;
    }
    if (surface)  { vkDestroySurfaceKHR(instance, surface, nullptr); surface  = VK_NULL_HANDLE; }
    if (instance) { vkDestroyInstance(instance, nullptr);            instance = VK_NULL_HANDLE; }

    if (window)   { glfwDestroyWindow(window); window = nullptr; }
    glfwTerminate();
}

} // namespace n64xr::ui
