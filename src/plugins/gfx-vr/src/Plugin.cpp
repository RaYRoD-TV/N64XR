// N64XR — gfx-vr plugin (Phase 1 scaffold)
//
// Implements the Mupen64Plus video plugin C ABI as no-op stubs that log every
// entry point. Loading this plugin into mupen64plus-core proves the ABI
// handshake works before real rendering exists.
//
// Subsequent work (Phase 1a workstream B in docs/plan.md) replaces the stubs
// in this file with a Vulkan backend driving the GLideN64 microcode translator,
// rendering into a swapchain handed to OpenXR via the vr-scene module.

#include "m64p_types.h"
#include "m64p_plugin.h"

#include <spdlog/spdlog.h>

namespace {

GFX_INFO g_gfxInfo{};
bool     g_started{false};

void log_init_once() {
    static bool s_initialised = false;
    if (s_initialised) return;
    s_initialised = true;
    spdlog::set_level(spdlog::level::debug);
    spdlog::set_pattern("[%H:%M:%S.%e] [%^%l%$] %v");
    spdlog::info("[gfx-vr] log initialised");
}

}  // namespace

// ---------------------------------------------------------------------------
// Common plugin entry points (m64p_common.h)
// ---------------------------------------------------------------------------

extern "C" {

EXPORT m64p_error CALL PluginStartup(m64p_dynlib_handle /*coreLib*/,
                                     void* /*debugContext*/,
                                     void (* /*debugCallback*/)(void*, int, const char*)) {
    log_init_once();
    spdlog::info("[gfx-vr] PluginStartup");
    g_started = true;
    return M64ERR_SUCCESS;
}

EXPORT m64p_error CALL PluginShutdown(void) {
    spdlog::info("[gfx-vr] PluginShutdown");
    g_started = false;
    return M64ERR_SUCCESS;
}

EXPORT m64p_error CALL PluginGetVersion(m64p_plugin_type* PluginType,
                                        int* PluginVersion,
                                        int* APIVersion,
                                        const char** PluginNamePtr,
                                        int* Capabilities) {
    if (PluginType)    *PluginType    = M64PLUGIN_GFX;
    if (PluginVersion) *PluginVersion = 0x000001;    // N64XR gfx-vr 0.0.1
    if (APIVersion)    *APIVersion    = 0x020200;    // Mupen64Plus video plugin ABI 2.2.0
    if (PluginNamePtr) *PluginNamePtr = "N64XR gfx-vr (Phase 1 scaffold)";
    if (Capabilities)  *Capabilities  = 0;
    return M64ERR_SUCCESS;
}

// ---------------------------------------------------------------------------
// Video plugin entry points (m64p_plugin.h — GFX section)
// ---------------------------------------------------------------------------

EXPORT int CALL InitiateGFX(GFX_INFO Gfx_Info) {
    g_gfxInfo = Gfx_Info;
    spdlog::info("[gfx-vr] InitiateGFX (HEADER={}, RDRAM={}, RDRAM_SIZE_ptr={})",
                 fmt::ptr(Gfx_Info.HEADER), fmt::ptr(Gfx_Info.RDRAM),
                 fmt::ptr(Gfx_Info.RDRAM_SIZE));
    // Phase 1a workstream B: bring up Vulkan device + queues + descriptor pool here.
    return 1;  // 1 == success in the M64P video ABI
}

EXPORT int  CALL RomOpen(void)     { spdlog::info ("[gfx-vr] RomOpen");     return 1; }
EXPORT void CALL RomClosed(void)   { spdlog::info ("[gfx-vr] RomClosed");           }
EXPORT void CALL ChangeWindow(void){ spdlog::debug("[gfx-vr] ChangeWindow");        }

EXPORT void CALL ProcessDList(void)    { spdlog::trace("[gfx-vr] ProcessDList");    }
EXPORT void CALL ProcessRDPList(void)  { spdlog::trace("[gfx-vr] ProcessRDPList");  }
EXPORT void CALL UpdateScreen(void)    { spdlog::trace("[gfx-vr] UpdateScreen");    }
EXPORT void CALL ShowCFB(void)         { spdlog::trace("[gfx-vr] ShowCFB");         }
EXPORT void CALL ViStatusChanged(void) { spdlog::debug("[gfx-vr] ViStatusChanged"); }
EXPORT void CALL ViWidthChanged(void)  { spdlog::debug("[gfx-vr] ViWidthChanged");  }

EXPORT void CALL MoveScreen(int x, int y) {
    spdlog::debug("[gfx-vr] MoveScreen({}, {})", x, y);
}

EXPORT void CALL ResizeVideoOutput(int width, int height) {
    spdlog::debug("[gfx-vr] ResizeVideoOutput({} x {})", width, height);
}

EXPORT void CALL ReadScreen2(void* dest, int* width, int* height, int front) {
    spdlog::trace("[gfx-vr] ReadScreen2(dest={}, front={})", fmt::ptr(dest), front);
    if (width)  *width  = 0;
    if (height) *height = 0;
    (void)dest;
}

EXPORT void CALL SetRenderingCallback(void (*callback)(int)) {
    spdlog::debug("[gfx-vr] SetRenderingCallback({})", fmt::ptr(reinterpret_cast<void*>(callback)));
}

EXPORT void CALL FBRead(unsigned int addr) {
    spdlog::trace("[gfx-vr] FBRead({:08x})", addr);
}

EXPORT void CALL FBWrite(unsigned int addr, unsigned int size) {
    spdlog::trace("[gfx-vr] FBWrite({:08x}, {})", addr, size);
}

EXPORT void CALL FBGetFrameBufferInfo(void* p) {
    spdlog::trace("[gfx-vr] FBGetFrameBufferInfo({})", fmt::ptr(p));
    (void)p;
}

}  // extern "C"
