// ============================================================================
//  HomeScreen.cpp — 'The Cabinet' lobby. Frosted instrument panels frame a
//  central negative-space void where the 3D holographic cartridge shows
//  through (rendered by HoloStage in the composite pass, behind ImGui).
// ----------------------------------------------------------------------------
//  Layout (fractions of the viewport work-area, resolution-independent):
//    * huge tracked ORBITRON title, top-left, generous margin
//    * LEFT  status column  (~24% wide)  — health + dim list
//    * RIGHT console readout (~26% wide)  — phosphor SYSTEM log
//    * CENTER ~46% left clear for the cartridge
//    * BOTTOM command bar (full width, short) — the control surface
//  The hero 'STEP INTO THE ROOM' button still breathes; the phosphor SYSTEM
//  copy + play-voiced labels are preserved.
// ============================================================================

#include "Screens.h"
#include "AppState.h"
#include "Theme.h"
#include "AliveIdle.h"
#include "GlassPanel.h"

#include <imgui.h>
#include <imgui_internal.h>
#include <spdlog/spdlog.h>

#include <cmath>

#include "XrSession.h"

namespace n64xr::ui {

using glass::Panel;
using glass::Scrim;
using glass::TextTracked;
using glass::MeasureTracked;

namespace {

inline ImU32 U32(const ImVec4& c) { return ImGui::ColorConvertFloat4ToU32(c); }
inline ImU32 U32a(const ImVec4& c, float a) {
    return ImGui::ColorConvertFloat4ToU32(ImVec4(c.x, c.y, c.z, a));
}

// The frosted fill tint — cool navy so warm brass accents pop.
const ImU32 kGlassTint = IM_COL32(18, 26, 40, 165);

} // namespace

// ---------------------------------------------------------------------------
//  Shared widget: breathing hero button (kept alive; now glass-framed).
// ---------------------------------------------------------------------------
bool BreathingButton(const char* label, float widthScale) {
    ImGuiWindow* w = ImGui::GetCurrentWindow();
    if (!w || w->SkipItems) return false;

    const float t      = static_cast<float>(ImGui::GetTime());
    const float breath = alive::Breath(t, 0.5f, 0.015f);

    ImFont* useFont = theme::GetFonts().displaySmall
                        ? theme::GetFonts().displaySmall
                        : ImGui::GetFont();
    ImGui::PushFont(useFont);

    const ImVec2 textSize = ImGui::CalcTextSize(label);
    const ImVec2 size = ImVec2((textSize.x + 72.0f) * widthScale * breath,
                               (textSize.y + 30.0f) * breath);

    const ImVec2 pos = ImGui::GetCursorScreenPos();
    const ImVec2 mn  = pos;
    const ImVec2 mx  = ImVec2(pos.x + size.x, pos.y + size.y);

    ImGui::InvisibleButton(label, size);
    const bool hovered = ImGui::IsItemHovered();
    const bool active  = ImGui::IsItemActive();
    const bool clicked = ImGui::IsItemClicked();

    ImDrawList* dl = ImGui::GetWindowDrawList();
    auto& pal = theme::Colours();

    const ImU32 glow = U32a(pal.brass, hovered ? 0.55f : 0.30f);
    alive::OuterGlow(dl, mn, mx, 8.0f, glow, hovered ? 14 : 8);

    const ImU32 top = U32(active ? pal.brassDim : (hovered ? pal.brassHot : pal.brass));
    const ImU32 bot = U32(ImVec4(pal.brassDim.x * 0.6f, pal.brassDim.y * 0.6f,
                                 pal.brassDim.z * 0.6f, 1.0f));
    dl->AddRectFilledMultiColor(mn, mx, top, top, bot, bot);
    dl->AddRect(mn, mx, U32(pal.brassHot), 8.0f, 0, 1.5f);

    alive::PanelBevel(dl, mn, mx, 8.0f, IM_COL32(255, 240, 200, 90), IM_COL32(0, 0, 0, 120));
    if (hovered)
        alive::MarchingAnts(dl, mn, mx, 8.0f, t, 80.0f, U32(pal.brassHot));

    const ImVec2 textPos = ImVec2(mn.x + (size.x - textSize.x) * 0.5f,
                                  mn.y + (size.y - textSize.y) * 0.5f);
    dl->AddText(ImVec2(textPos.x + 1, textPos.y + 1), IM_COL32(0, 0, 0, 180), label);
    dl->AddText(textPos, U32(pal.deepNavyBg), label);

    ImGui::PopFont();
    return clicked;
}

// ---------------------------------------------------------------------------
//  Phosphor status strip (bottom edge, above the command bar) — preserved.
// ---------------------------------------------------------------------------
void DrawPhosphorStatusStrip(const AppState& state) {
    ImGuiViewport* vp = ImGui::GetMainViewport();
    const float stripH = 36.0f;
    const ImVec2 stripMin(vp->WorkPos.x, vp->WorkPos.y + vp->WorkSize.y - stripH);
    const ImVec2 stripMax(vp->WorkPos.x + vp->WorkSize.x, vp->WorkPos.y + vp->WorkSize.y);

    ImDrawList* dl = ImGui::GetForegroundDrawList();
    auto& pal = theme::Colours();

    dl->AddRectFilled(stripMin, stripMax, U32(ImVec4(0.020f, 0.031f, 0.055f, 0.92f)));
    dl->AddLine(stripMin, ImVec2(stripMax.x, stripMin.y), U32(pal.brassDim), 1.0f);

    ImFont* f = theme::GetFonts().phosphor ? theme::GetFonts().phosphor : ImGui::GetFont();
    const float fontPx = f->LegacySize;
    const ImU32 green  = U32(pal.phosphor);
    const ImU32 greenD = U32(pal.phosphorDim);

    const float gx = stripMin.x + 18.0f;
    const float gy = stripMin.y + (stripH - fontPx) * 0.5f;
    dl->AddText(f, fontPx, ImVec2(gx + 1, gy + 1), greenD, state.statusLine.c_str());
    dl->AddText(f, fontPx, ImVec2(gx,     gy),     green,  state.statusLine.c_str());

    const ImVec2 rsz = f->CalcTextSizeA(fontPx, FLT_MAX, 0.0f, state.openxrRuntime.c_str());
    const ImVec2 rp(stripMax.x - rsz.x - 18.0f, gy);
    dl->AddText(f, fontPx, rp, greenD, state.openxrRuntime.c_str());
}

// ---------------------------------------------------------------------------
//  Smoke test (unchanged behaviour).
// ---------------------------------------------------------------------------
static void RunSmokeTest(AppState& s) {
    spdlog::info("Home: 'Step Into the Room' — booting OpenXR + 270 magenta frames.");
    s.statusLine = "Crossing the threshold...";
    n64xr::XrSession session;
    if (!session.initialize()) {
        spdlog::warn("XrSession::initialize() returned false — no headset?");
        s.statusLine        = "Headset did not answer. Cabinet returns to standby.";
        s.openxrRuntime     = "OpenXR runtime: unreachable.";
        s.vrSessionInFlight = false;
        return;
    }
    s.openxrRuntime = "OpenXR runtime: connected.";
    for (uint32_t i = 0; i < 270; ++i) {
        if (!session.pumpFrame()) break;
    }
    session.shutdown();
    s.statusLine        = "Returned from the room. 270 magenta frames delivered.";
    s.vrSessionInFlight = false;
    s.lastSmokeTestNote = "Smoke test complete.";
    spdlog::info("Smoke test complete.");
}

// ---------------------------------------------------------------------------
//  The Cabinet home screen.
// ---------------------------------------------------------------------------
void DrawHomeScreen(AppState& state) {
    auto& pal = theme::Colours();
    const auto& fonts = theme::GetFonts();
    ImGuiViewport* vp = ImGui::GetMainViewport();
    ImDrawList* dl = ImGui::GetWindowDrawList();

    // Work-area in screen space (account for the menu/tab spine UiHost already
    // consumed: we start from the current cursor for safe top margin).
    const ImVec2 origin = ImGui::GetCursorScreenPos();
    const float  W = ImGui::GetContentRegionAvail().x;
    const float  H = vp->WorkPos.y + vp->WorkSize.y - origin.y - 44.0f; // leave phosphor strip

    const float marginX = W * 0.035f;
    const float marginY = 18.0f;
    const float left    = origin.x + marginX;
    const float right   = origin.x + W - marginX;
    const float top     = origin.y + marginY;
    const float bottom  = origin.y + H - marginY;

    // ---- BIG title (tracked Orbitron) ----
    ImFont* disp = fonts.display ? fonts.display : ImGui::GetFont();
    const float titlePx = disp->LegacySize * 2.05f;   // size for a 4K display
    TextTracked(dl, disp, titlePx, ImVec2(left, top), U32(pal.agedPaper),
                "N64XR", titlePx * 0.10f);
    ImFont* dispS = fonts.displaySmall ? fonts.displaySmall : ImGui::GetFont();
    const float subPx = dispS->LegacySize * 1.05f;
    TextTracked(dl, dispS, subPx, ImVec2(left + 4.0f, top + titlePx + 6.0f),
                U32(pal.copper), "A CABINET FROM A FUTURE THAT NEVER HAPPENED", subPx * 0.14f);

    const float contentTop = top + titlePx + subPx + 40.0f;

    // ---- column geometry ----
    const float colGap   = W * 0.02f;
    const float leftW    = W * 0.24f;
    const float rightW   = W * 0.26f;
    const float cmdBarH  = 78.0f;
    const float colBot   = bottom - cmdBarH - 18.0f;

    const ImVec2 leftA (left,                 contentTop);
    const ImVec2 leftB (left + leftW,         colBot);
    const ImVec2 rightA(right - rightW,       contentTop);
    const ImVec2 rightB(right,                colBot);

    // ===================== LEFT STATUS COLUMN =====================
    Panel(dl, leftA, leftB, 10.0f, kGlassTint, U32(pal.brass), 0.8f);
    {
        ImFont* mono  = fonts.body  ? fonts.body  : ImGui::GetFont();
        ImFont* monoS = fonts.bodySmall ? fonts.bodySmall : mono;
        const float pad = 22.0f;
        float y = leftA.y + pad;

        TextTracked(dl, dispS, dispS->LegacySize * 0.92f, ImVec2(leftA.x + pad, y),
                    U32(pal.brassHot), "STATUS", 3.0f);
        y += dispS->LegacySize * 0.92f + 16.0f;
        dl->AddLine(ImVec2(leftA.x + pad, y), ImVec2(leftB.x - pad, y),
                    U32a(pal.brassDim, 0.6f), 1.0f);
        y += 14.0f;

        auto row = [&](const char* k, const char* v, ImU32 vc) {
            const float px = mono->LegacySize;
            dl->AddText(mono, px, ImVec2(leftA.x + pad, y), U32(pal.agedPaperDim), k);
            const ImVec2 vs = mono->CalcTextSizeA(px, FLT_MAX, 0.0f, v);
            dl->AddText(mono, px, ImVec2(leftB.x - pad - vs.x, y), vc, v);
            y += px + 12.0f;
        };
        row("power",    "OK",       U32(pal.phosphor));
        row("phosphor", "OK",       U32(pal.phosphor));
        row("vacuum",   "nominal",  U32(pal.phosphor));
        row("headset",  state.headsetStandingBy ? "standby" : "engaged",
            state.headsetStandingBy ? U32(pal.brassHot) : U32(pal.phosphor));

        y += 8.0f;
        dl->AddLine(ImVec2(leftA.x + pad, y), ImVec2(leftB.x - pad, y),
                    U32a(pal.brassDim, 0.4f), 1.0f);
        y += 14.0f;
        TextTracked(dl, monoS, monoS->LegacySize, ImVec2(leftA.x + pad, y),
                    U32(pal.agedPaperDim), "CARTRIDGE TRAY", 2.0f);
        y += monoS->LegacySize + 10.0f;
        const char* trayLine = state.roms.empty() ? "  ( empty — slot one )"
                                                  : "  loaded";
        dl->AddText(monoS, monoS->LegacySize, ImVec2(leftA.x + pad, y),
                    U32(pal.copper), trayLine);
    }

    // ===================== RIGHT CONSOLE READOUT =====================
    Panel(dl, rightA, rightB, 10.0f, kGlassTint, U32(pal.phosphor), 1.0f);
    {
        ImFont* ph = fonts.phosphor ? fonts.phosphor : ImGui::GetFont();
        const float px  = ph->LegacySize;
        const float pad = 22.0f;
        float y = rightA.y + pad;

        TextTracked(dl, dispS, dispS->LegacySize * 0.92f, ImVec2(rightA.x + pad, y),
                    U32(pal.phosphor), "CONSOLE READOUT", 2.0f);
        y += dispS->LegacySize * 0.92f + 16.0f;
        dl->AddLine(ImVec2(rightA.x + pad, y), ImVec2(rightB.x - pad, y),
                    U32a(pal.phosphorDim, 0.8f), 1.0f);
        y += 14.0f;

        auto line = [&](const char* s, ImU32 c) {
            dl->AddText(ph, px, ImVec2(rightA.x + pad + 1, y + 1), U32(pal.phosphorDim), s);
            dl->AddText(ph, px, ImVec2(rightA.x + pad,     y),     c, s);
            y += px + 4.0f;
        };
        line("> SYSTEM",            U32(pal.phosphor));
        line("  power.....OK",      U32(pal.phosphor));
        line("  phosphor..OK",      U32(pal.phosphor));
        line("  vacuum....nominal", U32(pal.phosphor));
        char hb[64];
        std::snprintf(hb, sizeof(hb), "  headset...%s",
                      state.headsetStandingBy ? "standing by" : "engaged");
        line(hb, U32(pal.phosphor));
        if (!state.lastSmokeTestNote.empty()) {
            y += 6.0f;
            char nb[128];
            std::snprintf(nb, sizeof(nb), "  note: %s", state.lastSmokeTestNote.c_str());
            line(nb, U32(pal.phosphorDim));
        }
    }

    // ===================== CENTER VOID (cartridge shows through) =====================
    // A faint scrim behind the title-area copy keeps it legible; the rest of
    // the centre is intentionally clear so the 3D hologram dominates.
    {
        const float cvA = leftB.x + colGap;
        const float cvB = rightA.x - colGap;
        (void)cvA; (void)cvB;
        // A barely-there caption floats under the cartridge.
        ImFont* monoS = fonts.bodySmall ? fonts.bodySmall : ImGui::GetFont();
        const char* cap = "HOLOGRAM · LIVE";
        const float capW = MeasureTracked(monoS, monoS->LegacySize, cap, 4.0f);
        const float capX = (leftB.x + rightA.x) * 0.5f - capW * 0.5f;
        const float capY = colBot - monoS->LegacySize - 14.0f;
        ImVec2 sa(capX - 18.0f, capY - 8.0f), sb(capX + capW + 18.0f, capY + monoS->LegacySize + 8.0f);
        Scrim(dl, sa, sb, IM_COL32(4, 7, 14, 120));
        TextTracked(dl, monoS, monoS->LegacySize, ImVec2(capX, capY),
                    U32a(pal.phosphor, 0.85f), cap, 4.0f);
    }

    // ===================== BOTTOM COMMAND BAR =====================
    const ImVec2 barA(left,  bottom - cmdBarH);
    const ImVec2 barB(right, bottom);
    Panel(dl, barA, barB, 10.0f, IM_COL32(24, 30, 44, 200), U32(pal.brassHot), 1.0f);

    // Place real (clickable) ImGui widgets on top of the painted bar.
    {
        const float pad = 22.0f;
        ImGui::SetCursorScreenPos(ImVec2(barA.x + pad, barA.y + (cmdBarH - 52.0f) * 0.5f));
        ImGui::BeginGroup();
        if (BreathingButton("  STEP INTO THE ROOM  ", 1.0f)) {
            state.vrSessionInFlight = true;
            RunSmokeTest(state);
        }
        ImGui::EndGroup();

        // Right-aligned secondary actions.
        ImGui::SameLine();
        const float secW = 420.0f;
        ImGui::SetCursorScreenPos(ImVec2(barB.x - secW - pad, barA.y + (cmdBarH - 40.0f) * 0.5f));
        ImGui::BeginGroup();
        if (ImGui::Button("  SLOT CARTRIDGE  "))            state.currentScreen = Screen::CartridgeVault;
        ImGui::SameLine();
        if (ImGui::Button("  PRY OPEN THE SERVICE HATCH  ")) state.currentScreen = Screen::ServiceHatch;
        ImGui::EndGroup();
    }

    // Reserve the layout space so ImGui's auto-cursor doesn't overlap the strip.
    ImGui::SetCursorScreenPos(ImVec2(origin.x, bottom + 6.0f));
    ImGui::Dummy(ImVec2(1, 1));
}

} // namespace n64xr::ui
