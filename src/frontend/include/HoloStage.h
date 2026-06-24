// ============================================================================
//  HoloStage.h — self-contained Vulkan module: holographic rotating N64
//  cartridge rendered to an offscreen HDR target, bloomed, and composited
//  (with a deep-space cabinet background) into the launcher's swapchain pass.
// ----------------------------------------------------------------------------
//  Owns ALL its own Vulkan objects. UiHost never touches anything inside; it
//  only calls the six entry points below. See the integration spec for the
//  exact wiring into UiHost's render loop.
//
//  Pass topology (all traditional VkRenderPass, single subpass each):
//    renderOffscreen():
//        1. cartridge   -> HDR colour (R16G16B16A16_SFLOAT) + D32 depth
//        2. bright-pass -> bloom mip[0] (half-res)
//        3. downsample  -> mip[1..N-1]
//        4. upsample    -> mip[N-2..0]  (additive)
//        ... all of the above happen BEFORE the swapchain render pass begins.
//    compositeFullscreen():
//        5. fullscreen triangle sampling (HDR scene + bloom mip[0]); paints
//           the space background, adds bloom, tonemaps. Runs INSIDE the
//           swapchain pass, as the first draw before ImGui.
// ============================================================================
#pragma once

#include <vulkan/vulkan.h>

#include <cstdint>
#include <string>
#include <vector>

namespace n64xr::holo {

class HoloStage {
public:
    HoloStage() = default;
    ~HoloStage() = default;

    HoloStage(const HoloStage&) = delete;
    HoloStage& operator=(const HoloStage&) = delete;

    // Create everything. `extent` is the swapchain extent (offscreen targets
    // match it). `shaderDir` is where the *.spv live at runtime (typically
    // "assets/shaders" relative to the working dir, like the fonts).
    // `swapColorFormat` is the swapchain's colour format so the composite
    // pipeline is built compatible with UiHost's render pass.
    void init(VkPhysicalDevice phys, VkDevice dev, uint32_t gfxFamily,
              VkQueue gfxQueue, VkCommandPool pool, VkExtent2D extent,
              VkFormat swapColorFormat, VkRenderPass swapchainRenderPass,
              const std::string& shaderDir);

    // Rebuild size-dependent resources after a swapchain resize.
    void resize(VkExtent2D extent);

    // Record the cartridge + bloom passes. MUST be called BEFORE the
    // swapchain render pass begins (it begins/ends its own render passes).
    void renderOffscreen(VkCommandBuffer cmd, float timeSeconds,
                         float parallaxX, float parallaxY);

    // Record the fullscreen composite. MUST be called INSIDE the swapchain
    // render pass, as the first draw before ImGui.
    void compositeFullscreen(VkCommandBuffer cmd, VkExtent2D swapExtent);

    void shutdown();

    bool initialised() const { return m_device != VK_NULL_HANDLE; }

private:
    // -------- small owned-image helper --------
    struct Image {
        VkImage        image  = VK_NULL_HANDLE;
        VkDeviceMemory memory = VK_NULL_HANDLE;
        VkImageView    view   = VK_NULL_HANDLE;
        uint32_t       width  = 0;
        uint32_t       height = 0;
        VkFormat       format = VK_FORMAT_UNDEFINED;
    };

    // -------- context (set once, survives resize) --------
    VkPhysicalDevice m_phys      = VK_NULL_HANDLE;
    VkDevice         m_device    = VK_NULL_HANDLE;
    uint32_t         m_gfxFamily = 0;
    VkQueue          m_gfxQueue  = VK_NULL_HANDLE;
    VkCommandPool    m_pool      = VK_NULL_HANDLE;
    VkExtent2D       m_extent    = { 0, 0 };
    VkFormat         m_swapFormat = VK_FORMAT_B8G8R8A8_UNORM;
    VkRenderPass     m_swapRenderPass = VK_NULL_HANDLE;
    std::string      m_shaderDir;

    static constexpr VkFormat kHdrFormat   = VK_FORMAT_R16G16B16A16_SFLOAT;
    static constexpr VkFormat kDepthFormat = VK_FORMAT_D32_SFLOAT;
    static constexpr uint32_t kBloomMips   = 6;

    // -------- size-dependent targets --------
    Image m_hdr;          // cartridge colour target
    Image m_depth;        // cartridge depth
    std::vector<Image> m_bloom; // kBloomMips levels, half/quarter/...

    // framebuffers (size-dependent)
    VkFramebuffer m_cartridgeFb = VK_NULL_HANDLE;
    std::vector<VkFramebuffer> m_bloomFb; // one per mip level

    // -------- size-independent objects (survive resize) --------
    VkRenderPass m_cartridgePass = VK_NULL_HANDLE; // HDR + depth
    VkRenderPass m_bloomPass     = VK_NULL_HANDLE; // HDR colour only, no blend
    VkRenderPass m_bloomAddPass  = VK_NULL_HANDLE; // HDR colour, LOAD + additive

    VkSampler m_linearClamp = VK_NULL_HANDLE;

    // descriptors
    VkDescriptorPool      m_descPool          = VK_NULL_HANDLE;
    VkDescriptorSetLayout m_uboSetLayout      = VK_NULL_HANDLE; // cartridge UBO
    VkDescriptorSetLayout m_singleTexLayout   = VK_NULL_HANDLE; // 1 sampler
    VkDescriptorSetLayout m_dualTexLayout     = VK_NULL_HANDLE; // 2 samplers (composite)

    VkDescriptorSet m_uboSet      = VK_NULL_HANDLE;
    VkDescriptorSet m_brightSet   = VK_NULL_HANDLE;          // samples HDR
    std::vector<VkDescriptorSet> m_downSets;                 // [i] samples mip[i]
    std::vector<VkDescriptorSet> m_upSets;                   // [i] samples mip[i+1]
    VkDescriptorSet m_compositeSet = VK_NULL_HANDLE;         // HDR + bloom[0]

    // pipelines + layouts
    VkPipelineLayout m_cartridgeLayout = VK_NULL_HANDLE;
    VkPipelineLayout m_bloomLayout     = VK_NULL_HANDLE;     // push-const + 1 tex
    VkPipelineLayout m_compositeLayout = VK_NULL_HANDLE;     // push-const + 2 tex
    VkPipeline m_cartridgePipe = VK_NULL_HANDLE;
    VkPipeline m_brightPipe    = VK_NULL_HANDLE;
    VkPipeline m_downPipe      = VK_NULL_HANDLE;
    VkPipeline m_upPipe        = VK_NULL_HANDLE;
    VkPipeline m_compositePipe = VK_NULL_HANDLE;

    // geometry
    VkBuffer       m_vbuf = VK_NULL_HANDLE;
    VkDeviceMemory m_vmem = VK_NULL_HANDLE;
    VkBuffer       m_ibuf = VK_NULL_HANDLE;
    VkDeviceMemory m_imem = VK_NULL_HANDLE;
    uint32_t       m_indexCount = 0;

    // uniform buffer (host-visible, persistently mapped)
    VkBuffer       m_ubo    = VK_NULL_HANDLE;
    VkDeviceMemory m_uboMem = VK_NULL_HANDLE;
    void*          m_uboMapped = nullptr;

    // -------- internal helpers --------
    uint32_t findMemoryType(uint32_t typeBits, VkMemoryPropertyFlags props) const;
    void createImage(Image& img, uint32_t w, uint32_t h, VkFormat fmt,
                     VkImageUsageFlags usage, VkImageAspectFlags aspect);
    void destroyImage(Image& img);
    VkShaderModule loadShader(const std::string& file) const;
    void createBuffer(VkDeviceSize size, VkBufferUsageFlags usage,
                      VkMemoryPropertyFlags props,
                      VkBuffer& buf, VkDeviceMemory& mem) const;
    void uploadGeometry();
    void createRenderPasses();
    void createSamplerAndDescLayouts();
    void createPipelines();
    void createSizedTargets();
    void destroySizedTargets();
    void allocateDescriptors();
    void updateDescriptors();
    void barrierToSampled(VkCommandBuffer cmd, VkImage img) const;
};

} // namespace n64xr::holo
