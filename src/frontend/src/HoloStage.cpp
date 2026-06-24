// ============================================================================
//  HoloStage.cpp — offscreen holographic cartridge + bloom + composite.
// ----------------------------------------------------------------------------
//  Manual Vulkan (no VMA dependency in the frontend target). Traditional
//  VkRenderPasses to mirror UiHost's style. All layout transitions handled by
//  render-pass initial/final layouts + EXTERNAL subpass dependencies so we
//  don't hand-roll a barrier per pass (the bloom chain reads SHADER_READ_ONLY
//  and each pass leaves its attachment there for the next pass to sample).
// ============================================================================

#include "HoloStage.h"
#include "Cartridge.h"
#include "HoloMath.h"

#include <spdlog/spdlog.h>

#include <array>
#include <cstring>
#include <fstream>
#include <stdexcept>

namespace n64xr::holo {
namespace {

void VkOk(VkResult r, const char* what) {
    if (r != VK_SUCCESS) {
        throw std::runtime_error(std::string("HoloStage Vulkan: ") + what +
                                 " failed (VkResult=" + std::to_string(int(r)) + ")");
    }
}

// std140-friendly UBO mirror. Keep in lockstep with cartridge.vert/.frag.
struct CartridgeUBO {
    float model[16];
    float view[16];
    float proj[16];
    float normalMat[16];
    float camPos[4];      // xyz + pad
    float timeParams[4];  // x=time, y=glitchAmp, z=sweepSpeed, w=unused
};

// Push constants shared by bloom passes (32 bytes max usage).
struct BloomPC {
    float texelSize[2];   // 1/source size
    float threshold;      // bright-pass only
    float knee;           // bright-pass only
    float filterRadius;   // upsample only
    float pad0, pad1, pad2;
};

struct CompositePC {
    float invResolution[2];
    float time;
    float bloomStrength;
};

uint32_t mipDim(uint32_t base, uint32_t level) {
    uint32_t d = base >> level;
    return d < 1u ? 1u : d;
}

} // namespace

// ----------------------------------------------------------------------------
//  Memory + image + buffer helpers
// ----------------------------------------------------------------------------
uint32_t HoloStage::findMemoryType(uint32_t typeBits, VkMemoryPropertyFlags props) const {
    VkPhysicalDeviceMemoryProperties mp{};
    vkGetPhysicalDeviceMemoryProperties(m_phys, &mp);
    for (uint32_t i = 0; i < mp.memoryTypeCount; ++i) {
        if ((typeBits & (1u << i)) &&
            (mp.memoryTypes[i].propertyFlags & props) == props) {
            return i;
        }
    }
    throw std::runtime_error("HoloStage: no suitable memory type.");
}

void HoloStage::createImage(Image& img, uint32_t w, uint32_t h, VkFormat fmt,
                            VkImageUsageFlags usage, VkImageAspectFlags aspect) {
    img.width = w; img.height = h; img.format = fmt;

    VkImageCreateInfo ici{ VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
    ici.imageType     = VK_IMAGE_TYPE_2D;
    ici.format        = fmt;
    ici.extent        = { w, h, 1 };
    ici.mipLevels     = 1;
    ici.arrayLayers   = 1;
    ici.samples       = VK_SAMPLE_COUNT_1_BIT;
    ici.tiling        = VK_IMAGE_TILING_OPTIMAL;
    ici.usage         = usage;
    ici.sharingMode   = VK_SHARING_MODE_EXCLUSIVE;
    ici.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    VkOk(vkCreateImage(m_device, &ici, nullptr, &img.image), "vkCreateImage");

    VkMemoryRequirements req{};
    vkGetImageMemoryRequirements(m_device, img.image, &req);
    VkMemoryAllocateInfo mai{ VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
    mai.allocationSize  = req.size;
    mai.memoryTypeIndex = findMemoryType(req.memoryTypeBits,
                                         VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    VkOk(vkAllocateMemory(m_device, &mai, nullptr, &img.memory), "vkAllocateMemory (image)");
    VkOk(vkBindImageMemory(m_device, img.image, img.memory, 0), "vkBindImageMemory");

    VkImageViewCreateInfo ivci{ VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
    ivci.image            = img.image;
    ivci.viewType         = VK_IMAGE_VIEW_TYPE_2D;
    ivci.format           = fmt;
    ivci.subresourceRange = { aspect, 0, 1, 0, 1 };
    VkOk(vkCreateImageView(m_device, &ivci, nullptr, &img.view), "vkCreateImageView");
}

void HoloStage::destroyImage(Image& img) {
    if (img.view)   { vkDestroyImageView(m_device, img.view, nullptr);   img.view   = VK_NULL_HANDLE; }
    if (img.image)  { vkDestroyImage(m_device, img.image, nullptr);      img.image  = VK_NULL_HANDLE; }
    if (img.memory) { vkFreeMemory(m_device, img.memory, nullptr);       img.memory = VK_NULL_HANDLE; }
}

void HoloStage::createBuffer(VkDeviceSize size, VkBufferUsageFlags usage,
                             VkMemoryPropertyFlags props,
                             VkBuffer& buf, VkDeviceMemory& mem) const {
    VkBufferCreateInfo bci{ VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
    bci.size        = size;
    bci.usage       = usage;
    bci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    VkOk(vkCreateBuffer(m_device, &bci, nullptr, &buf), "vkCreateBuffer");

    VkMemoryRequirements req{};
    vkGetBufferMemoryRequirements(m_device, buf, &req);
    VkMemoryAllocateInfo mai{ VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
    mai.allocationSize  = req.size;
    mai.memoryTypeIndex = findMemoryType(req.memoryTypeBits, props);
    VkOk(vkAllocateMemory(m_device, &mai, nullptr, &mem), "vkAllocateMemory (buffer)");
    VkOk(vkBindBufferMemory(m_device, buf, mem, 0), "vkBindBufferMemory");
}

VkShaderModule HoloStage::loadShader(const std::string& file) const {
    const std::string path = m_shaderDir + "/" + file;
    std::ifstream f(path, std::ios::binary | std::ios::ate);
    if (!f) throw std::runtime_error("HoloStage: cannot open shader " + path);
    const std::streamsize sz = f.tellg();
    f.seekg(0);
    std::vector<char> data(static_cast<size_t>(sz));
    f.read(data.data(), sz);
    if (data.size() % 4 != 0)
        throw std::runtime_error("HoloStage: shader not 4-byte aligned: " + path);

    VkShaderModuleCreateInfo smci{ VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO };
    smci.codeSize = data.size();
    smci.pCode    = reinterpret_cast<const uint32_t*>(data.data());
    VkShaderModule mod = VK_NULL_HANDLE;
    VkOk(vkCreateShaderModule(m_device, &smci, nullptr, &mod), "vkCreateShaderModule");
    return mod;
}

// ----------------------------------------------------------------------------
//  Geometry upload (staging via host-visible, one-time submit copy)
// ----------------------------------------------------------------------------
void HoloStage::uploadGeometry() {
    std::vector<Vertex>   verts;
    std::vector<uint32_t> indices;
    BuildCartridge(verts, indices);
    m_indexCount = static_cast<uint32_t>(indices.size());

    const VkDeviceSize vsize = sizeof(Vertex) * verts.size();
    const VkDeviceSize isize = sizeof(uint32_t) * indices.size();

    // Device-local destination buffers.
    createBuffer(vsize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, m_vbuf, m_vmem);
    createBuffer(isize, VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, m_ibuf, m_imem);

    // Staging buffers (host-visible).
    VkBuffer vstage, istage; VkDeviceMemory vstageMem, istageMem;
    createBuffer(vsize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                 vstage, vstageMem);
    createBuffer(isize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                 istage, istageMem);

    void* p = nullptr;
    vkMapMemory(m_device, vstageMem, 0, vsize, 0, &p);
    std::memcpy(p, verts.data(), static_cast<size_t>(vsize));
    vkUnmapMemory(m_device, vstageMem);
    vkMapMemory(m_device, istageMem, 0, isize, 0, &p);
    std::memcpy(p, indices.data(), static_cast<size_t>(isize));
    vkUnmapMemory(m_device, istageMem);

    // One-time copy.
    VkCommandBufferAllocateInfo cbai{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
    cbai.commandPool        = m_pool;
    cbai.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cbai.commandBufferCount = 1;
    VkCommandBuffer cmd = VK_NULL_HANDLE;
    VkOk(vkAllocateCommandBuffers(m_device, &cbai, &cmd), "alloc copy cmd");

    VkCommandBufferBeginInfo bi{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
    bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(cmd, &bi);
    VkBufferCopy vcopy{ 0, 0, vsize };
    VkBufferCopy icopy{ 0, 0, isize };
    vkCmdCopyBuffer(cmd, vstage, m_vbuf, 1, &vcopy);
    vkCmdCopyBuffer(cmd, istage, m_ibuf, 1, &icopy);
    vkEndCommandBuffer(cmd);

    VkSubmitInfo si{ VK_STRUCTURE_TYPE_SUBMIT_INFO };
    si.commandBufferCount = 1;
    si.pCommandBuffers    = &cmd;
    VkOk(vkQueueSubmit(m_gfxQueue, 1, &si, VK_NULL_HANDLE), "copy submit");
    VkOk(vkQueueWaitIdle(m_gfxQueue), "copy wait");

    vkFreeCommandBuffers(m_device, m_pool, 1, &cmd);
    vkDestroyBuffer(m_device, vstage, nullptr); vkFreeMemory(m_device, vstageMem, nullptr);
    vkDestroyBuffer(m_device, istage, nullptr); vkFreeMemory(m_device, istageMem, nullptr);

    // Uniform buffer (persistently mapped).
    createBuffer(sizeof(CartridgeUBO), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                 m_ubo, m_uboMem);
    vkMapMemory(m_device, m_uboMem, 0, sizeof(CartridgeUBO), 0, &m_uboMapped);
}

// ----------------------------------------------------------------------------
//  Render passes
// ----------------------------------------------------------------------------
void HoloStage::createRenderPasses() {
    // ---- Cartridge pass: HDR colour (CLEAR) + depth ----
    {
        VkAttachmentDescription atts[2]{};
        atts[0].format         = kHdrFormat;
        atts[0].samples        = VK_SAMPLE_COUNT_1_BIT;
        atts[0].loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
        atts[0].storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
        atts[0].stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        atts[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        atts[0].initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
        atts[0].finalLayout    = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        atts[1].format         = kDepthFormat;
        atts[1].samples        = VK_SAMPLE_COUNT_1_BIT;
        atts[1].loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
        atts[1].storeOp        = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        atts[1].stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        atts[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        atts[1].initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
        atts[1].finalLayout    = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        VkAttachmentReference colRef{ 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };
        VkAttachmentReference depRef{ 1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL };
        VkSubpassDescription sub{};
        sub.pipelineBindPoint       = VK_PIPELINE_BIND_POINT_GRAPHICS;
        sub.colorAttachmentCount    = 1;
        sub.pColorAttachments       = &colRef;
        sub.pDepthStencilAttachment = &depRef;

        std::array<VkSubpassDependency, 2> deps{};
        // external -> subpass: sync the colour AND depth attachment clears with
        // the layout transitions (the depth half was missing -> WRITE-AFTER-WRITE
        // hazard, garbage depth, every fragment discarded).
        deps[0].srcSubpass    = VK_SUBPASS_EXTERNAL;
        deps[0].dstSubpass    = 0;
        deps[0].srcStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                                VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT |
                                VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
        deps[0].dstStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                                VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT |
                                VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
        deps[0].srcAccessMask = 0;
        deps[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
                                VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        deps[1].srcSubpass    = 0;
        deps[1].dstSubpass    = VK_SUBPASS_EXTERNAL;
        deps[1].srcStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        deps[1].dstStageMask  = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        deps[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        deps[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

        VkRenderPassCreateInfo rpci{ VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO };
        rpci.attachmentCount = 2;
        rpci.pAttachments    = atts;
        rpci.subpassCount    = 1;
        rpci.pSubpasses      = &sub;
        rpci.dependencyCount = static_cast<uint32_t>(deps.size());
        rpci.pDependencies   = deps.data();
        VkOk(vkCreateRenderPass(m_device, &rpci, nullptr, &m_cartridgePass), "cartridge RP");
    }

    // ---- Bloom pass (overwrite): single HDR colour, no blend ----
    auto makeBloomPass = [&](VkAttachmentLoadOp loadOp, VkRenderPass& out) {
        VkAttachmentDescription att{};
        att.format         = kHdrFormat;
        att.samples        = VK_SAMPLE_COUNT_1_BIT;
        att.loadOp         = loadOp;
        att.storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
        att.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        att.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        att.initialLayout  = (loadOp == VK_ATTACHMENT_LOAD_OP_LOAD)
                               ? VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
                               : VK_IMAGE_LAYOUT_UNDEFINED;
        att.finalLayout    = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        VkAttachmentReference ref{ 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };
        VkSubpassDescription sub{};
        sub.pipelineBindPoint    = VK_PIPELINE_BIND_POINT_GRAPHICS;
        sub.colorAttachmentCount = 1;
        sub.pColorAttachments    = &ref;

        std::array<VkSubpassDependency, 2> deps{};
        deps[0].srcSubpass    = VK_SUBPASS_EXTERNAL;
        deps[0].dstSubpass    = 0;
        deps[0].srcStageMask  = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        deps[0].dstStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        deps[0].srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
        deps[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
                                VK_ACCESS_COLOR_ATTACHMENT_READ_BIT;
        deps[1].srcSubpass    = 0;
        deps[1].dstSubpass    = VK_SUBPASS_EXTERNAL;
        deps[1].srcStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        deps[1].dstStageMask  = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        deps[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        deps[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

        VkRenderPassCreateInfo rpci{ VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO };
        rpci.attachmentCount = 1;
        rpci.pAttachments    = &att;
        rpci.subpassCount    = 1;
        rpci.pSubpasses      = &sub;
        rpci.dependencyCount = static_cast<uint32_t>(deps.size());
        rpci.pDependencies   = deps.data();
        VkOk(vkCreateRenderPass(m_device, &rpci, nullptr, &out), "bloom RP");
    };
    makeBloomPass(VK_ATTACHMENT_LOAD_OP_DONT_CARE, m_bloomPass);    // bright/down: overwrite
    makeBloomPass(VK_ATTACHMENT_LOAD_OP_LOAD,      m_bloomAddPass); // upsample: additive onto existing
}

// ----------------------------------------------------------------------------
//  Sampler + descriptor set layouts
// ----------------------------------------------------------------------------
void HoloStage::createSamplerAndDescLayouts() {
    VkSamplerCreateInfo sci{ VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO };
    sci.magFilter    = VK_FILTER_LINEAR;
    sci.minFilter    = VK_FILTER_LINEAR;
    sci.mipmapMode   = VK_SAMPLER_MIPMAP_MODE_NEAREST;
    sci.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sci.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sci.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sci.maxLod       = 0.0f;
    sci.borderColor  = VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK;
    VkOk(vkCreateSampler(m_device, &sci, nullptr, &m_linearClamp), "vkCreateSampler");

    auto makeLayout = [&](uint32_t count, VkDescriptorType type,
                          VkShaderStageFlags stage, VkDescriptorSetLayout& out) {
        std::vector<VkDescriptorSetLayoutBinding> binds(count);
        for (uint32_t i = 0; i < count; ++i) {
            binds[i].binding         = i;
            binds[i].descriptorType  = type;
            binds[i].descriptorCount = 1;
            binds[i].stageFlags      = stage;
        }
        VkDescriptorSetLayoutCreateInfo lci{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
        lci.bindingCount = count;
        lci.pBindings    = binds.data();
        VkOk(vkCreateDescriptorSetLayout(m_device, &lci, nullptr, &out), "desc layout");
    };
    makeLayout(1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
               VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, m_uboSetLayout);
    makeLayout(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
               VK_SHADER_STAGE_FRAGMENT_BIT, m_singleTexLayout);
    makeLayout(2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
               VK_SHADER_STAGE_FRAGMENT_BIT, m_dualTexLayout);
}

// ----------------------------------------------------------------------------
//  Pipelines
// ----------------------------------------------------------------------------
void HoloStage::createPipelines() {
    // ---------- pipeline layouts ----------
    {
        VkPipelineLayoutCreateInfo plci{ VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
        plci.setLayoutCount = 1;
        plci.pSetLayouts    = &m_uboSetLayout;
        VkOk(vkCreatePipelineLayout(m_device, &plci, nullptr, &m_cartridgeLayout), "cart layout");
    }
    {
        VkPushConstantRange pc{ VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(BloomPC) };
        VkPipelineLayoutCreateInfo plci{ VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
        plci.setLayoutCount         = 1;
        plci.pSetLayouts            = &m_singleTexLayout;
        plci.pushConstantRangeCount = 1;
        plci.pPushConstantRanges    = &pc;
        VkOk(vkCreatePipelineLayout(m_device, &plci, nullptr, &m_bloomLayout), "bloom layout");
    }
    {
        VkPushConstantRange pc{ VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(CompositePC) };
        VkPipelineLayoutCreateInfo plci{ VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
        plci.setLayoutCount         = 1;
        plci.pSetLayouts            = &m_dualTexLayout;
        plci.pushConstantRangeCount = 1;
        plci.pPushConstantRanges    = &pc;
        VkOk(vkCreatePipelineLayout(m_device, &plci, nullptr, &m_compositeLayout), "composite layout");
    }

    // ---------- shared fixed state ----------
    VkPipelineInputAssemblyStateCreateInfo ia{ VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO };
    ia.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    VkPipelineViewportStateCreateInfo vp{ VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO };
    vp.viewportCount = 1;
    vp.scissorCount  = 1;

    const VkDynamicState dynStates[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
    VkPipelineDynamicStateCreateInfo dyn{ VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO };
    dyn.dynamicStateCount = 2;
    dyn.pDynamicStates    = dynStates;

    VkPipelineMultisampleStateCreateInfo ms{ VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO };
    ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    auto makeRaster = [](VkCullModeFlags cull) {
        VkPipelineRasterizationStateCreateInfo rs{ VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO };
        rs.polygonMode = VK_POLYGON_MODE_FILL;
        rs.cullMode    = cull;
        rs.frontFace   = VK_FRONT_FACE_COUNTER_CLOCKWISE;
        rs.lineWidth   = 1.0f;
        return rs;
    };

    // ---------- cartridge pipeline ----------
    {
        VkShaderModule vs = loadShader("cartridge.vert.spv");
        VkShaderModule fs = loadShader("cartridge.frag.spv");
        VkPipelineShaderStageCreateInfo stages[2]{};
        stages[0] = { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO };
        stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;   stages[0].module = vs; stages[0].pName = "main";
        stages[1] = { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO };
        stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT; stages[1].module = fs; stages[1].pName = "main";

        VkVertexInputBindingDescription bind{ 0, sizeof(Vertex), VK_VERTEX_INPUT_RATE_VERTEX };
        std::array<VkVertexInputAttributeDescription, 3> attrs{};
        attrs[0] = { 0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, px) };
        attrs[1] = { 1, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, nx) };
        attrs[2] = { 2, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, bx) };
        VkPipelineVertexInputStateCreateInfo vi{ VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO };
        vi.vertexBindingDescriptionCount   = 1;
        vi.pVertexBindingDescriptions      = &bind;
        vi.vertexAttributeDescriptionCount = static_cast<uint32_t>(attrs.size());
        vi.pVertexAttributeDescriptions    = attrs.data();

        VkPipelineDepthStencilStateCreateInfo ds{ VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO };
        // Translucent wireframe hologram: NO depth test. We WANT the far edges
        // to show through the near faces, and it makes the render independent of
        // the depth attachment's (unsynchronised) clear state.
        ds.depthTestEnable  = VK_FALSE;
        ds.depthWriteEnable = VK_FALSE;
        ds.depthCompareOp   = VK_COMPARE_OP_ALWAYS;

        // alpha blend (ghost-glass hologram)
        VkPipelineColorBlendAttachmentState cba{};
        cba.blendEnable         = VK_TRUE;
        cba.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        cba.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        cba.colorBlendOp        = VK_BLEND_OP_ADD;
        cba.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        cba.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        cba.alphaBlendOp        = VK_BLEND_OP_ADD;
        cba.colorWriteMask      = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                  VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        VkPipelineColorBlendStateCreateInfo cb{ VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO };
        cb.attachmentCount = 1;
        cb.pAttachments    = &cba;

        auto rs = makeRaster(VK_CULL_MODE_NONE); // see back faces through the hologram
        VkGraphicsPipelineCreateInfo gp{ VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO };
        gp.stageCount          = 2;
        gp.pStages             = stages;
        gp.pVertexInputState   = &vi;
        gp.pInputAssemblyState = &ia;
        gp.pViewportState      = &vp;
        gp.pRasterizationState = &rs;
        gp.pMultisampleState   = &ms;
        gp.pDepthStencilState  = &ds;
        gp.pColorBlendState    = &cb;
        gp.pDynamicState       = &dyn;
        gp.layout              = m_cartridgeLayout;
        gp.renderPass          = m_cartridgePass;
        gp.subpass             = 0;
        VkOk(vkCreateGraphicsPipelines(m_device, VK_NULL_HANDLE, 1, &gp, nullptr, &m_cartridgePipe),
             "cartridge pipeline");
        vkDestroyShaderModule(m_device, vs, nullptr);
        vkDestroyShaderModule(m_device, fs, nullptr);
    }

    // ---------- fullscreen-triangle helper (no vertex input) ----------
    VkPipelineVertexInputStateCreateInfo emptyVi{ VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO };
    VkPipelineDepthStencilStateCreateInfo noDepth{ VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO };

    auto makeFullscreenPipe = [&](const char* fragFile, VkPipelineLayout layout,
                                  VkRenderPass pass, bool additive,
                                  VkPipeline& outPipe) {
        VkShaderModule vs = loadShader("fullscreen.vert.spv");
        VkShaderModule fs = loadShader(fragFile);
        VkPipelineShaderStageCreateInfo stages[2]{};
        stages[0] = { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO };
        stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;   stages[0].module = vs; stages[0].pName = "main";
        stages[1] = { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO };
        stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT; stages[1].module = fs; stages[1].pName = "main";

        VkPipelineColorBlendAttachmentState cba{};
        cba.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                             VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        if (additive) {
            cba.blendEnable         = VK_TRUE;
            cba.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
            cba.dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
            cba.colorBlendOp        = VK_BLEND_OP_ADD;
            cba.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
            cba.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
            cba.alphaBlendOp        = VK_BLEND_OP_ADD;
        } else {
            cba.blendEnable = VK_FALSE;
        }
        VkPipelineColorBlendStateCreateInfo cb{ VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO };
        cb.attachmentCount = 1;
        cb.pAttachments    = &cba;

        auto rs = makeRaster(VK_CULL_MODE_NONE);
        VkGraphicsPipelineCreateInfo gp{ VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO };
        gp.stageCount          = 2;
        gp.pStages             = stages;
        gp.pVertexInputState   = &emptyVi;
        gp.pInputAssemblyState = &ia;
        gp.pViewportState      = &vp;
        gp.pRasterizationState = &rs;
        gp.pMultisampleState   = &ms;
        gp.pDepthStencilState  = &noDepth;
        gp.pColorBlendState    = &cb;
        gp.pDynamicState       = &dyn;
        gp.layout              = layout;
        gp.renderPass          = pass;
        gp.subpass             = 0;
        VkOk(vkCreateGraphicsPipelines(m_device, VK_NULL_HANDLE, 1, &gp, nullptr, &outPipe),
             "fullscreen pipeline");
        vkDestroyShaderModule(m_device, vs, nullptr);
        vkDestroyShaderModule(m_device, fs, nullptr);
    };

    makeFullscreenPipe("brightpass.frag.spv", m_bloomLayout,     m_bloomPass,    false, m_brightPipe);
    makeFullscreenPipe("blur.frag.spv",       m_bloomLayout,     m_bloomPass,    false, m_downPipe);
    makeFullscreenPipe("upsample.frag.spv",   m_bloomLayout,     m_bloomAddPass, true,  m_upPipe);
    makeFullscreenPipe("composite.frag.spv",  m_compositeLayout, m_swapRenderPass, false, m_compositePipe);
}

// ----------------------------------------------------------------------------
//  Sized targets (HDR, depth, bloom chain) + framebuffers
// ----------------------------------------------------------------------------
void HoloStage::createSizedTargets() {
    createImage(m_hdr, m_extent.width, m_extent.height, kHdrFormat,
                VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                VK_IMAGE_ASPECT_COLOR_BIT);
    createImage(m_depth, m_extent.width, m_extent.height, kDepthFormat,
                VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
                VK_IMAGE_ASPECT_DEPTH_BIT);

    // bloom mip[0] = half-res; each subsequent halves again.
    m_bloom.resize(kBloomMips);
    m_bloomFb.resize(kBloomMips);
    for (uint32_t i = 0; i < kBloomMips; ++i) {
        const uint32_t w = mipDim(m_extent.width,  i + 1);
        const uint32_t h = mipDim(m_extent.height, i + 1);
        createImage(m_bloom[i], w, h, kHdrFormat,
                    VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                    VK_IMAGE_ASPECT_COLOR_BIT);
    }

    // cartridge framebuffer (colour + depth)
    {
        VkImageView atts[2] = { m_hdr.view, m_depth.view };
        VkFramebufferCreateInfo fbci{ VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO };
        fbci.renderPass      = m_cartridgePass;
        fbci.attachmentCount = 2;
        fbci.pAttachments    = atts;
        fbci.width           = m_extent.width;
        fbci.height          = m_extent.height;
        fbci.layers          = 1;
        VkOk(vkCreateFramebuffer(m_device, &fbci, nullptr, &m_cartridgeFb), "cartridge FB");
    }
    // bloom framebuffers (use m_bloomPass; m_bloomAddPass is renderpass-compatible)
    for (uint32_t i = 0; i < kBloomMips; ++i) {
        VkFramebufferCreateInfo fbci{ VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO };
        fbci.renderPass      = m_bloomPass;
        fbci.attachmentCount = 1;
        fbci.pAttachments    = &m_bloom[i].view;
        fbci.width           = m_bloom[i].width;
        fbci.height          = m_bloom[i].height;
        fbci.layers          = 1;
        VkOk(vkCreateFramebuffer(m_device, &fbci, nullptr, &m_bloomFb[i]), "bloom FB");
    }
}

void HoloStage::destroySizedTargets() {
    if (m_cartridgeFb) { vkDestroyFramebuffer(m_device, m_cartridgeFb, nullptr); m_cartridgeFb = VK_NULL_HANDLE; }
    for (auto fb : m_bloomFb) if (fb) vkDestroyFramebuffer(m_device, fb, nullptr);
    m_bloomFb.clear();
    for (auto& img : m_bloom) destroyImage(img);
    m_bloom.clear();
    destroyImage(m_depth);
    destroyImage(m_hdr);
}

// ----------------------------------------------------------------------------
//  Descriptors
// ----------------------------------------------------------------------------
void HoloStage::allocateDescriptors() {
    // Pool sized generously: UBO(1) + per-frame samplers.
    const uint32_t texSets = 1 /*bright*/ + (kBloomMips - 1) /*down*/ +
                             (kBloomMips - 1) /*up*/ + 1 /*composite (2 binds)*/;
    std::array<VkDescriptorPoolSize, 2> sizes{};
    sizes[0] = { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1 };
    sizes[1] = { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, texSets * 2 + 4 };
    VkDescriptorPoolCreateInfo dpci{ VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
    dpci.flags         = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    dpci.maxSets       = texSets + 4;
    dpci.poolSizeCount = static_cast<uint32_t>(sizes.size());
    dpci.pPoolSizes    = sizes.data();
    VkOk(vkCreateDescriptorPool(m_device, &dpci, nullptr, &m_descPool), "holo desc pool");

    auto alloc = [&](VkDescriptorSetLayout layout) {
        VkDescriptorSetAllocateInfo ai{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
        ai.descriptorPool     = m_descPool;
        ai.descriptorSetCount = 1;
        ai.pSetLayouts        = &layout;
        VkDescriptorSet set = VK_NULL_HANDLE;
        VkOk(vkAllocateDescriptorSets(m_device, &ai, &set), "alloc desc set");
        return set;
    };

    m_uboSet    = alloc(m_uboSetLayout);
    m_brightSet = alloc(m_singleTexLayout);
    m_downSets.resize(kBloomMips - 1);
    m_upSets.resize(kBloomMips - 1);
    for (uint32_t i = 0; i < kBloomMips - 1; ++i) m_downSets[i] = alloc(m_singleTexLayout);
    for (uint32_t i = 0; i < kBloomMips - 1; ++i) m_upSets[i]   = alloc(m_singleTexLayout);
    m_compositeSet = alloc(m_dualTexLayout);

    // UBO descriptor (size-independent buffer; write once).
    VkDescriptorBufferInfo bi{ m_ubo, 0, sizeof(CartridgeUBO) };
    VkWriteDescriptorSet w{ VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
    w.dstSet          = m_uboSet;
    w.dstBinding      = 0;
    w.descriptorCount = 1;
    w.descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    w.pBufferInfo     = &bi;
    vkUpdateDescriptorSets(m_device, 1, &w, 0, nullptr);
}

void HoloStage::updateDescriptors() {
    auto writeTex = [&](VkDescriptorSet set, uint32_t binding, VkImageView view) {
        VkDescriptorImageInfo ii{};
        ii.sampler     = m_linearClamp;
        ii.imageView   = view;
        ii.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        VkWriteDescriptorSet w{ VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
        w.dstSet          = set;
        w.dstBinding      = binding;
        w.descriptorCount = 1;
        w.descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        w.pImageInfo      = &ii;
        vkUpdateDescriptorSets(m_device, 1, &w, 0, nullptr);
    };

    writeTex(m_brightSet, 0, m_hdr.view);                 // bright-pass samples scene
    for (uint32_t i = 0; i < kBloomMips - 1; ++i)
        writeTex(m_downSets[i], 0, m_bloom[i].view);      // down i: sample mip[i] -> write mip[i+1]
    for (uint32_t i = 0; i < kBloomMips - 1; ++i)
        writeTex(m_upSets[i], 0, m_bloom[i + 1].view);    // up i: sample mip[i+1] -> add into mip[i]

    writeTex(m_compositeSet, 0, m_hdr.view);              // composite binding 0 = scene
    writeTex(m_compositeSet, 1, m_bloom[0].view);         // composite binding 1 = bloom
}

// ----------------------------------------------------------------------------
//  Public lifecycle
// ----------------------------------------------------------------------------
void HoloStage::init(VkPhysicalDevice phys, VkDevice dev, uint32_t gfxFamily,
                     VkQueue gfxQueue, VkCommandPool pool, VkExtent2D extent,
                     VkFormat swapColorFormat, VkRenderPass swapchainRenderPass,
                     const std::string& shaderDir) {
    m_phys           = phys;
    m_device         = dev;
    m_gfxFamily      = gfxFamily;
    m_gfxQueue       = gfxQueue;
    m_pool           = pool;
    m_extent         = extent;
    m_swapFormat     = swapColorFormat;
    m_swapRenderPass = swapchainRenderPass;
    m_shaderDir      = shaderDir;

    uploadGeometry();
    createRenderPasses();
    createSamplerAndDescLayouts();
    createPipelines();
    createSizedTargets();
    allocateDescriptors();
    updateDescriptors();

    spdlog::info("HoloStage: holographic cartridge stage ready ({}x{}, {} bloom mips).",
                 extent.width, extent.height, kBloomMips);
}

void HoloStage::resize(VkExtent2D extent) {
    if (!m_device) return;
    vkDeviceWaitIdle(m_device);
    m_extent = extent;
    destroySizedTargets();
    createSizedTargets();
    updateDescriptors(); // repoint sampler descriptors at the new views
}

void HoloStage::barrierToSampled(VkCommandBuffer, VkImage) const {
    // Layout transitions are handled by render-pass initial/final layouts +
    // EXTERNAL subpass dependencies; no explicit barrier needed. Kept for
    // documentation symmetry.
}

// ----------------------------------------------------------------------------
//  renderOffscreen — cartridge + bloom (BEFORE the swapchain pass)
// ----------------------------------------------------------------------------
void HoloStage::renderOffscreen(VkCommandBuffer cmd, float timeSeconds,
                                float parallaxX, float parallaxY) {
    // ---- update UBO ----
    {
        using namespace n64xr::holo;
        const float aspect = (m_extent.height > 0)
                               ? float(m_extent.width) / float(m_extent.height) : 1.0f;

        // Auto-rotate around Y; gentle mouse-parallax tilt on X/Y.
        const float yaw   = timeSeconds * 0.45f;
        const float tiltX = parallaxY * 0.28f;       // look up/down
        const float tiltY = parallaxX * 0.30f;       // extra yaw nudge

        Mat4 model = mul(rotateY(yaw + tiltY), rotateX(tiltX));

        const Vec3 eye{ 0.0f, 0.05f, 2.35f };
        const Vec3 center{ 0.0f, 0.0f, 0.0f };
        const Vec3 up{ 0.0f, 1.0f, 0.0f };
        Mat4 view = lookAt(eye, center, up);
        Mat4 proj = perspective(0.80f /*~46deg*/, aspect, 0.05f, 50.0f);

        // For a pure rotation, normal matrix == model (orthonormal). Fine here.
        Mat4 normalMat = model;

        CartridgeUBO ubo{};
        std::memcpy(ubo.model,     model.m,     sizeof(ubo.model));
        std::memcpy(ubo.view,      view.m,      sizeof(ubo.view));
        std::memcpy(ubo.proj,      proj.m,      sizeof(ubo.proj));
        std::memcpy(ubo.normalMat, normalMat.m, sizeof(ubo.normalMat));
        ubo.camPos[0] = eye.x; ubo.camPos[1] = eye.y; ubo.camPos[2] = eye.z; ubo.camPos[3] = 1.0f;
        ubo.timeParams[0] = timeSeconds;
        ubo.timeParams[1] = 0.012f;   // glitchAmp (subtle)
        ubo.timeParams[2] = 0.22f;    // sweepSpeed (slow, dignified)
        ubo.timeParams[3] = 0.0f;
        std::memcpy(m_uboMapped, &ubo, sizeof(ubo));
    }

    auto setVpScissor = [&](uint32_t w, uint32_t h) {
        VkViewport vpr{ 0.0f, 0.0f, float(w), float(h), 0.0f, 1.0f };
        VkRect2D sc{ {0,0}, {w,h} };
        vkCmdSetViewport(cmd, 0, 1, &vpr);
        vkCmdSetScissor(cmd, 0, 1, &sc);
    };

    // ---- 1. cartridge -> HDR ----
    {
        VkClearValue clears[2]{};
        clears[0].color        = { { 0.0f, 0.0f, 0.0f, 0.0f } }; // transparent void
        clears[1].depthStencil = { 1.0f, 0 };
        VkRenderPassBeginInfo rp{ VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO };
        rp.renderPass        = m_cartridgePass;
        rp.framebuffer       = m_cartridgeFb;
        rp.renderArea.extent = m_extent;
        rp.clearValueCount   = 2;
        rp.pClearValues      = clears;
        vkCmdBeginRenderPass(cmd, &rp, VK_SUBPASS_CONTENTS_INLINE);
        setVpScissor(m_extent.width, m_extent.height);
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_cartridgePipe);
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_cartridgeLayout,
                                0, 1, &m_uboSet, 0, nullptr);
        VkDeviceSize off = 0;
        vkCmdBindVertexBuffers(cmd, 0, 1, &m_vbuf, &off);
        vkCmdBindIndexBuffer(cmd, m_ibuf, 0, VK_INDEX_TYPE_UINT32);
        vkCmdDrawIndexed(cmd, m_indexCount, 1, 0, 0, 0);
        vkCmdEndRenderPass(cmd);
    }

    // ---- 2. bright-pass: HDR -> bloom[0] ----
    {
        VkRenderPassBeginInfo rp{ VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO };
        rp.renderPass        = m_bloomPass;
        rp.framebuffer       = m_bloomFb[0];
        rp.renderArea.extent = { m_bloom[0].width, m_bloom[0].height };
        vkCmdBeginRenderPass(cmd, &rp, VK_SUBPASS_CONTENTS_INLINE);
        setVpScissor(m_bloom[0].width, m_bloom[0].height);
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_brightPipe);
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_bloomLayout,
                                0, 1, &m_brightSet, 0, nullptr);
        BloomPC pc{};
        pc.texelSize[0] = 1.0f / float(m_hdr.width);
        pc.texelSize[1] = 1.0f / float(m_hdr.height);
        pc.threshold    = 0.65f;
        pc.knee         = 0.45f;
        vkCmdPushConstants(cmd, m_bloomLayout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(pc), &pc);
        vkCmdDraw(cmd, 3, 1, 0, 0);
        vkCmdEndRenderPass(cmd);
    }

    // ---- 3. downsample: mip[i] -> mip[i+1] ----
    for (uint32_t i = 0; i < kBloomMips - 1; ++i) {
        VkRenderPassBeginInfo rp{ VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO };
        rp.renderPass        = m_bloomPass;
        rp.framebuffer       = m_bloomFb[i + 1];
        rp.renderArea.extent = { m_bloom[i + 1].width, m_bloom[i + 1].height };
        vkCmdBeginRenderPass(cmd, &rp, VK_SUBPASS_CONTENTS_INLINE);
        setVpScissor(m_bloom[i + 1].width, m_bloom[i + 1].height);
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_downPipe);
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_bloomLayout,
                                0, 1, &m_downSets[i], 0, nullptr);
        BloomPC pc{};
        pc.texelSize[0] = 1.0f / float(m_bloom[i].width);   // SOURCE mip texel size
        pc.texelSize[1] = 1.0f / float(m_bloom[i].height);
        vkCmdPushConstants(cmd, m_bloomLayout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(pc), &pc);
        vkCmdDraw(cmd, 3, 1, 0, 0);
        vkCmdEndRenderPass(cmd);
    }

    // ---- 4. upsample: mip[i+1] -> additively into mip[i] ----
    for (int i = int(kBloomMips) - 2; i >= 0; --i) {
        VkRenderPassBeginInfo rp{ VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO };
        rp.renderPass        = m_bloomAddPass;   // LOAD existing + additive blend
        rp.framebuffer       = m_bloomFb[i];
        rp.renderArea.extent = { m_bloom[i].width, m_bloom[i].height };
        vkCmdBeginRenderPass(cmd, &rp, VK_SUBPASS_CONTENTS_INLINE);
        setVpScissor(m_bloom[i].width, m_bloom[i].height);
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_upPipe);
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_bloomLayout,
                                0, 1, &m_upSets[i], 0, nullptr);
        BloomPC pc{};
        pc.texelSize[0]  = 1.0f / float(m_bloom[i + 1].width);  // SOURCE (smaller) mip
        pc.texelSize[1]  = 1.0f / float(m_bloom[i + 1].height);
        pc.filterRadius  = 1.0f;
        vkCmdPushConstants(cmd, m_bloomLayout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(pc), &pc);
        vkCmdDraw(cmd, 3, 1, 0, 0);
        vkCmdEndRenderPass(cmd);
    }
    // After this, m_hdr and m_bloom[0] are both SHADER_READ_ONLY_OPTIMAL.
}

// ----------------------------------------------------------------------------
//  compositeFullscreen — INSIDE the swapchain pass, first draw before ImGui
// ----------------------------------------------------------------------------
void HoloStage::compositeFullscreen(VkCommandBuffer cmd, VkExtent2D swapExtent) {
    VkViewport vpr{ 0.0f, 0.0f, float(swapExtent.width), float(swapExtent.height), 0.0f, 1.0f };
    VkRect2D sc{ {0,0}, swapExtent };
    vkCmdSetViewport(cmd, 0, 1, &vpr);
    vkCmdSetScissor(cmd, 0, 1, &sc);

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_compositePipe);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_compositeLayout,
                            0, 1, &m_compositeSet, 0, nullptr);
    CompositePC pc{};
    pc.invResolution[0] = 1.0f / float(swapExtent.width);
    pc.invResolution[1] = 1.0f / float(swapExtent.height);
    pc.time             = float(0.0); // time animated in starfield via fragCoord hash + below
    pc.bloomStrength    = 0.85f;
    vkCmdPushConstants(cmd, m_compositeLayout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(pc), &pc);
    vkCmdDraw(cmd, 3, 1, 0, 0);
}

// ----------------------------------------------------------------------------
//  Shutdown — reverse order, drain first.
// ----------------------------------------------------------------------------
void HoloStage::shutdown() {
    if (!m_device) return;
    vkDeviceWaitIdle(m_device);

    destroySizedTargets();

    if (m_descPool)   { vkDestroyDescriptorPool(m_device, m_descPool, nullptr); m_descPool = VK_NULL_HANDLE; }

    auto destroyPipe   = [&](VkPipeline& p)        { if (p) { vkDestroyPipeline(m_device, p, nullptr); p = VK_NULL_HANDLE; } };
    auto destroyLayout = [&](VkPipelineLayout& l)  { if (l) { vkDestroyPipelineLayout(m_device, l, nullptr); l = VK_NULL_HANDLE; } };
    auto destroyDSL    = [&](VkDescriptorSetLayout& l){ if (l) { vkDestroyDescriptorSetLayout(m_device, l, nullptr); l = VK_NULL_HANDLE; } };
    auto destroyRP     = [&](VkRenderPass& r)       { if (r) { vkDestroyRenderPass(m_device, r, nullptr); r = VK_NULL_HANDLE; } };

    destroyPipe(m_compositePipe);
    destroyPipe(m_upPipe);
    destroyPipe(m_downPipe);
    destroyPipe(m_brightPipe);
    destroyPipe(m_cartridgePipe);

    destroyLayout(m_compositeLayout);
    destroyLayout(m_bloomLayout);
    destroyLayout(m_cartridgeLayout);

    destroyDSL(m_dualTexLayout);
    destroyDSL(m_singleTexLayout);
    destroyDSL(m_uboSetLayout);

    if (m_linearClamp) { vkDestroySampler(m_device, m_linearClamp, nullptr); m_linearClamp = VK_NULL_HANDLE; }

    destroyRP(m_bloomAddPass);
    destroyRP(m_bloomPass);
    destroyRP(m_cartridgePass);

    if (m_uboMapped) { vkUnmapMemory(m_device, m_uboMem); m_uboMapped = nullptr; }
    if (m_ubo)  { vkDestroyBuffer(m_device, m_ubo, nullptr);  m_ubo  = VK_NULL_HANDLE; }
    if (m_uboMem){ vkFreeMemory(m_device, m_uboMem, nullptr); m_uboMem = VK_NULL_HANDLE; }
    if (m_vbuf) { vkDestroyBuffer(m_device, m_vbuf, nullptr); m_vbuf = VK_NULL_HANDLE; }
    if (m_vmem) { vkFreeMemory(m_device, m_vmem, nullptr);    m_vmem = VK_NULL_HANDLE; }
    if (m_ibuf) { vkDestroyBuffer(m_device, m_ibuf, nullptr); m_ibuf = VK_NULL_HANDLE; }
    if (m_imem) { vkFreeMemory(m_device, m_imem, nullptr);    m_imem = VK_NULL_HANDLE; }

    m_device = VK_NULL_HANDLE;
}

} // namespace n64xr::holo
