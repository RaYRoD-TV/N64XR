// ============================================================================
//  ArtCache.h — load LOCAL cover/cartridge art (the user's own LaunchBox media
//  or any art folder) and expose it as ImGui textures for the carousel.
// ----------------------------------------------------------------------------
//  The app reads whatever image files exist on the user's disk at runtime
//  (exactly like LaunchBox / RetroArch). NOTHING is bundled or shipped — the
//  repo carries no art. Matching is by normalised game title.
// ============================================================================
#pragma once

#include <imgui.h>
#include <vulkan/vulkan.h>

#include <cstdint>
#include <string>

namespace n64xr::ui {

class ArtCache {
public:
    // Vulkan handles needed to upload textures + register them with the
    // ImGui Vulkan backend. Call once after the device + ImGui are up.
    void init(VkPhysicalDevice phys, VkDevice dev, VkQueue queue,
              uint32_t gfxFamily, VkCommandPool pool);

    // Point at an art root. If `explicitRoot` is empty, derive it from the ROM
    // scan path (LaunchBox layout: .../Games/<Platform> -> .../Images/<Platform>).
    // Builds a normalised filename index.
    void setRoots(const std::string& romScanPath, const std::string& explicitRoot = "");

    // Texture for a game by its (ROM) display name. Lazy-loads + caches.
    // Returns 0 + leaves w/h at 0 if no art matches. aspect = w/h.
    ImTextureID get(const std::string& gameName, int* outW = nullptr, int* outH = nullptr);

    bool ready() const { return m_device != VK_NULL_HANDLE; }
    void shutdown();

private:
    struct Impl;
    Impl* m_impl = nullptr;
    VkDevice m_device = VK_NULL_HANDLE;
};

// Process-wide instance (the carousel reaches it without plumbing).
ArtCache& Art();

} // namespace n64xr::ui
