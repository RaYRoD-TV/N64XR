// ============================================================================
//  ArtCache.cpp — decode local image files (stb_image) -> Vulkan textures ->
//  ImGui texture ids, matched to games by normalised title.
// ============================================================================

#include "ArtCache.h"

#include <imgui_impl_vulkan.h>
#include <spdlog/spdlog.h>

#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_JPEG
#define STBI_ONLY_PNG
#include <stb_image.h>

#include <algorithm>
#include <cctype>
#include <cstring>
#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

namespace n64xr::ui {
namespace fs = std::filesystem;

namespace {

// Normalise a title to a match key: drop anything in (...) or [...], lowercase,
// keep only [a-z0-9]. "007 - The World Is Not Enough (USA)" and
// "007_ The World Is Not Enough" both collapse to "007theworldisnotenough".
std::string normKey(const std::string& in) {
    std::string s;
    int depth = 0;
    for (char c : in) {
        if (c == '(' || c == '[') { ++depth; continue; }
        if (c == ')' || c == ']') { if (depth) --depth; continue; }
        if (depth) continue;
        if (std::isalnum(static_cast<unsigned char>(c)))
            s += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return s;
}

// Strip a trailing "-01" / "-2" image-variant suffix before normalising.
std::string stemNoVariant(const fs::path& p) {
    std::string s = p.stem().string();
    auto dash = s.find_last_of('-');
    if (dash != std::string::npos) {
        bool allDigit = dash + 1 < s.size();
        for (size_t i = dash + 1; i < s.size(); ++i)
            if (!std::isdigit(static_cast<unsigned char>(s[i]))) { allDigit = false; break; }
        if (allDigit) s = s.substr(0, dash);
    }
    return s;
}

} // namespace

struct Tex {
    VkImage        image = VK_NULL_HANDLE;
    VkDeviceMemory mem   = VK_NULL_HANDLE;
    VkImageView    view  = VK_NULL_HANDLE;
    VkDescriptorSet ds   = VK_NULL_HANDLE;
    int w = 0, h = 0;
    bool tried = false;   // attempted load (so we don't retry misses)
};

struct ArtCache::Impl {
    VkPhysicalDevice phys = VK_NULL_HANDLE;
    VkDevice         dev  = VK_NULL_HANDLE;
    VkQueue          queue = VK_NULL_HANDLE;
    uint32_t         family = 0;
    VkCommandPool    pool  = VK_NULL_HANDLE;
    VkSampler        sampler = VK_NULL_HANDLE;

    std::unordered_map<std::string, std::string> index; // normKey -> filepath
    std::unordered_map<std::string, Tex>          cache; // filepath -> texture

    uint32_t findMem(uint32_t bits, VkMemoryPropertyFlags props) const {
        VkPhysicalDeviceMemoryProperties mp{};
        vkGetPhysicalDeviceMemoryProperties(phys, &mp);
        for (uint32_t i = 0; i < mp.memoryTypeCount; ++i)
            if ((bits & (1u << i)) && (mp.memoryTypes[i].propertyFlags & props) == props)
                return i;
        return 0;
    }

    Tex upload(const std::string& path) {
        Tex t; t.tried = true;
        int w = 0, h = 0, comp = 0;
        stbi_uc* px = stbi_load(path.c_str(), &w, &h, &comp, 4);
        if (!px) { spdlog::debug("[art] decode failed: {}", path); return t; }
        const VkDeviceSize bytes = VkDeviceSize(w) * h * 4;

        // staging
        VkBuffer sbuf; VkDeviceMemory smem;
        {
            VkBufferCreateInfo bi{ VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
            bi.size = bytes; bi.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
            bi.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
            vkCreateBuffer(dev, &bi, nullptr, &sbuf);
            VkMemoryRequirements req{}; vkGetBufferMemoryRequirements(dev, sbuf, &req);
            VkMemoryAllocateInfo ai{ VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
            ai.allocationSize = req.size;
            ai.memoryTypeIndex = findMem(req.memoryTypeBits,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
            vkAllocateMemory(dev, &ai, nullptr, &smem);
            vkBindBufferMemory(dev, sbuf, smem, 0);
            void* map = nullptr; vkMapMemory(dev, smem, 0, bytes, 0, &map);
            std::memcpy(map, px, size_t(bytes)); vkUnmapMemory(dev, smem);
        }
        stbi_image_free(px);

        // image
        VkImageCreateInfo ici{ VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
        ici.imageType = VK_IMAGE_TYPE_2D;
        ici.format = VK_FORMAT_R8G8B8A8_UNORM;
        ici.extent = { uint32_t(w), uint32_t(h), 1 };
        ici.mipLevels = 1; ici.arrayLayers = 1;
        ici.samples = VK_SAMPLE_COUNT_1_BIT; ici.tiling = VK_IMAGE_TILING_OPTIMAL;
        ici.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        ici.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        ici.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        vkCreateImage(dev, &ici, nullptr, &t.image);
        VkMemoryRequirements ireq{}; vkGetImageMemoryRequirements(dev, t.image, &ireq);
        VkMemoryAllocateInfo iai{ VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
        iai.allocationSize = ireq.size;
        iai.memoryTypeIndex = findMem(ireq.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        vkAllocateMemory(dev, &iai, nullptr, &t.mem);
        vkBindImageMemory(dev, t.image, t.mem, 0);

        // one-shot copy + transitions
        VkCommandBufferAllocateInfo cai{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
        cai.commandPool = pool; cai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY; cai.commandBufferCount = 1;
        VkCommandBuffer cmd; vkAllocateCommandBuffers(dev, &cai, &cmd);
        VkCommandBufferBeginInfo bi{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
        bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        vkBeginCommandBuffer(cmd, &bi);

        auto barrier = [&](VkImageLayout o, VkImageLayout n, VkAccessFlags sa, VkAccessFlags da,
                           VkPipelineStageFlags ss, VkPipelineStageFlags ds) {
            VkImageMemoryBarrier b{ VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
            b.oldLayout = o; b.newLayout = n; b.srcAccessMask = sa; b.dstAccessMask = da;
            b.image = t.image; b.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
            b.srcQueueFamilyIndex = b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            vkCmdPipelineBarrier(cmd, ss, ds, 0, 0, nullptr, 0, nullptr, 1, &b);
        };
        barrier(VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                0, VK_ACCESS_TRANSFER_WRITE_BIT,
                VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);
        VkBufferImageCopy region{};
        region.imageSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
        region.imageExtent = { uint32_t(w), uint32_t(h), 1 };
        vkCmdCopyBufferToImage(cmd, sbuf, t.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
        barrier(VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
                VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
        vkEndCommandBuffer(cmd);
        VkSubmitInfo si{ VK_STRUCTURE_TYPE_SUBMIT_INFO };
        si.commandBufferCount = 1; si.pCommandBuffers = &cmd;
        vkQueueSubmit(queue, 1, &si, VK_NULL_HANDLE);
        vkQueueWaitIdle(queue);
        vkFreeCommandBuffers(dev, pool, 1, &cmd);
        vkDestroyBuffer(dev, sbuf, nullptr); vkFreeMemory(dev, smem, nullptr);

        // view + imgui id
        VkImageViewCreateInfo vci{ VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
        vci.image = t.image; vci.viewType = VK_IMAGE_VIEW_TYPE_2D;
        vci.format = VK_FORMAT_R8G8B8A8_UNORM;
        vci.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
        vkCreateImageView(dev, &vci, nullptr, &t.view);
        t.ds = ImGui_ImplVulkan_AddTexture(t.view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        t.w = w; t.h = h;
        return t;
    }
};

ArtCache& Art() { static ArtCache s; return s; }

void ArtCache::init(VkPhysicalDevice phys, VkDevice dev, VkQueue queue,
                    uint32_t gfxFamily, VkCommandPool pool) {
    m_device = dev;
    m_impl = new Impl();
    m_impl->phys = phys; m_impl->dev = dev; m_impl->queue = queue;
    m_impl->family = gfxFamily; m_impl->pool = pool;

    VkSamplerCreateInfo sci{ VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO };
    sci.magFilter = VK_FILTER_LINEAR; sci.minFilter = VK_FILTER_LINEAR;
    sci.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    sci.addressModeU = sci.addressModeV = sci.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sci.maxLod = 1.0f; sci.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK;
    vkCreateSampler(dev, &sci, nullptr, &m_impl->sampler);
}

void ArtCache::setRoots(const std::string& romScanPath, const std::string& explicitRoot) {
    if (!m_impl) return;
    m_impl->index.clear();

    std::vector<fs::path> roots;
    if (!explicitRoot.empty()) {
        roots.push_back(explicitRoot);
    } else {
        // LaunchBox: .../Games/<Platform>  ->  .../Images/<Platform>/{Box - Front, Cart - 3D, ...}
        std::string s = romScanPath;
        const std::string from = "\\Games\\", to = "\\Images\\";
        auto pos = s.find(from);
        if (pos == std::string::npos) { const std::string f2 = "/Games/"; pos = s.find(f2);
            if (pos != std::string::npos) s.replace(pos, f2.size(), "/Images/"); }
        else s.replace(pos, from.size(), to);
        fs::path platform(s);
        for (const char* sub : { "Box - Front", "Cart - 3D", "Cart - Front", "Box - 3D" })
            roots.push_back(platform / sub);
    }

    std::error_code ec;
    int count = 0;
    for (const auto& root : roots) {
        if (!fs::exists(root, ec)) continue;
        for (const auto& e : fs::recursive_directory_iterator(root,
                                 fs::directory_options::skip_permission_denied, ec)) {
            if (ec) { ec.clear(); continue; }
            if (!e.is_regular_file(ec)) continue;
            std::string ext = e.path().extension().string();
            std::transform(ext.begin(), ext.end(), ext.begin(),
                           [](unsigned char c){ return char(std::tolower(c)); });
            if (ext != ".jpg" && ext != ".jpeg" && ext != ".png") continue;
            std::string key = normKey(stemNoVariant(e.path()));
            if (key.empty()) continue;
            m_impl->index.emplace(key, e.path().string()); // first wins (-01 sorts first)
            ++count;
        }
    }
    spdlog::info("[art] indexed {} images across {} root(s).", count, roots.size());
}

ImTextureID ArtCache::get(const std::string& gameName, int* outW, int* outH) {
    if (outW) *outW = 0; if (outH) *outH = 0;
    if (!m_impl) return 0;
    auto it = m_impl->index.find(normKey(gameName));
    if (it == m_impl->index.end()) return 0;
    const std::string& path = it->second;

    auto c = m_impl->cache.find(path);
    if (c == m_impl->cache.end())
        c = m_impl->cache.emplace(path, m_impl->upload(path)).first;
    const Tex& t = c->second;
    if (!t.ds) return 0;
    if (outW) *outW = t.w; if (outH) *outH = t.h;
    return (ImTextureID)t.ds;
}

void ArtCache::shutdown() {
    if (!m_impl) return;
    for (auto& kv : m_impl->cache) {
        Tex& t = kv.second;
        if (t.ds)    ImGui_ImplVulkan_RemoveTexture(t.ds);
        if (t.view)  vkDestroyImageView(m_impl->dev, t.view, nullptr);
        if (t.image) vkDestroyImage(m_impl->dev, t.image, nullptr);
        if (t.mem)   vkFreeMemory(m_impl->dev, t.mem, nullptr);
    }
    if (m_impl->sampler) vkDestroySampler(m_impl->dev, m_impl->sampler, nullptr);
    delete m_impl; m_impl = nullptr;
    m_device = VK_NULL_HANDLE;
}

} // namespace n64xr::ui
