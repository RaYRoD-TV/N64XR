// ============================================================================
//  Cartridge.cpp — procedural N64 cartridge geometry.
// ----------------------------------------------------------------------------
//  Real N64 carts are PORTRAIT — ~89 x 119 x 22 mm  ->  W:H:D ~ 0.75 : 1.0 : 0.18.
//  We exaggerate the chamfer / label inset / notch slightly so they read in
//  wireframe. All faces emitted via Tri() which stamps a per-face normal +
//  barycentric corner, so the whole mesh is wireframe-ready.
// ============================================================================

#include "Cartridge.h"
#include "HoloMath.h"

#include <array>
#include <cmath>

namespace n64xr::holo {
namespace {

struct Builder {
    std::vector<Vertex>&   v;
    std::vector<uint32_t>& idx;

    void tri(const Vec3& a, const Vec3& b, const Vec3& c) {
        Vec3 n = normalize(cross(sub(b, a), sub(c, a)));
        uint32_t base = static_cast<uint32_t>(v.size());
        v.push_back({ a.x, a.y, a.z, n.x, n.y, n.z, 1, 0, 0 });
        v.push_back({ b.x, b.y, b.z, n.x, n.y, n.z, 0, 1, 0 });
        v.push_back({ c.x, c.y, c.z, n.x, n.y, n.z, 0, 0, 1 });
        idx.push_back(base);
        idx.push_back(base + 1);
        idx.push_back(base + 2);
    }
    // CCW quad (a,b,c,d) -> two tris.
    void quad(const Vec3& a, const Vec3& b, const Vec3& c, const Vec3& d) {
        tri(a, b, c);
        tri(a, c, d);
    }
};

inline Vec3 P(float x, float y, float z) { return { x, y, z }; }

} // namespace

void BuildCartridge(std::vector<Vertex>& outVerts,
                    std::vector<uint32_t>& outIndices) {
    outVerts.clear();
    outIndices.clear();
    Builder b{ outVerts, outIndices };

    // --- Canonical proportions (width = 1.0) ---
    const float W = 0.75f, H = 1.0f, D = 0.15f;   // portrait, like a real N64 cart
    const float hw = W * 0.5f, hh = H * 0.5f, hd = D * 0.5f;
    const float cham  = 0.085f * H;   // top-corner chamfer (exaggerated)
    const float taper = 0.035f * W;   // per-side bottom inset

    // 8-point front outline (XY plane), CCW: tapered bottom, chamfered top.
    struct V2 { float x, y; };
    const std::array<V2, 8> o = { {
        { -hw + taper, -hh        },   // 0 bottom-left
        {  hw - taper, -hh        },   // 1 bottom-right
        {  hw,         -hh + cham },   // 2 right-lower
        {  hw,          hh - cham },   // 3 right-upper
        {  hw - cham,   hh        },   // 4 top-right (chamfer)
        { -hw + cham,   hh        },   // 5 top-left  (chamfer)
        { -hw,          hh - cham },   // 6 left-upper
        { -hw,         -hh + cham },   // 7 left-lower
    } };

    // ---- Pass 1: side wall ring (front z=+hd .. back z=-hd) ----
    for (size_t i = 0; i < o.size(); ++i) {
        const V2 p0 = o[i];
        const V2 p1 = o[(i + 1) % o.size()];
        b.quad(P(p0.x, p0.y, +hd), P(p1.x, p1.y, +hd),
               P(p1.x, p1.y, -hd), P(p0.x, p0.y, -hd));
    }

    // ---- Back cap (fan from outline centroid), normal -Z ----
    {
        const Vec3 cen = P(0.0f, 0.0f, -hd);
        for (size_t i = 0; i < o.size(); ++i) {
            const V2 p0 = o[i];
            const V2 p1 = o[(i + 1) % o.size()];
            // wind so the face normal points -Z (outward at the back)
            b.tri(cen, P(p1.x, p1.y, -hd), P(p0.x, p0.y, -hd));
        }
    }

    // ---- Pass 2: FRONT face = frame ring around a recessed label panel ----
    const float labMx  = 0.10f * W;     // side margin
    const float labTop = 0.07f * H;     // gap from top edge
    const float labBot = 0.42f * H;     // label occupies upper band
    const float labDep = 0.34f * D;     // recess depth
    const float lx0 = -hw + labMx, lx1 = hw - labMx;
    const float ly0 = hh - labBot, ly1 = hh - labTop;   // ly0 < ly1

    // Frame ring: front face minus the label rectangle. Triangulate as a fan
    // of quads from each outline edge to the nearest label-rect corner band.
    // Simpler + robust: build the frame as 4 border quads around the rect,
    // clipped to the outline's bounding interior. For a clean wireframe we use
    // the rectangle inset and connect to the outline with a triangle fan that
    // respects the chamfers.
    {
        // Front outer fan to label-rect: emit a ring of quads.
        // Label rect corners (front plane).
        const Vec3 r00 = P(lx0, ly0, +hd);
        const Vec3 r10 = P(lx1, ly0, +hd);
        const Vec3 r11 = P(lx1, ly1, +hd);
        const Vec3 r01 = P(lx0, ly1, +hd);

        // Connect each outline vertex to the closest rect corner, building a
        // closed frame. We pair outline edges with rect edges quadrant-wise.
        // Bottom border (outline pts 0,1 -> r00,r10)
        b.quad(P(o[0].x, o[0].y, +hd), P(o[1].x, o[1].y, +hd), r10, r00);
        // Right border (outline pts 1,2,3,4 -> r10,r11)
        b.tri(P(o[1].x, o[1].y, +hd), P(o[2].x, o[2].y, +hd), r10);
        b.tri(P(o[2].x, o[2].y, +hd), P(o[3].x, o[3].y, +hd), r10);
        b.tri(P(o[3].x, o[3].y, +hd), r11, r10);
        b.tri(P(o[3].x, o[3].y, +hd), P(o[4].x, o[4].y, +hd), r11);
        // Top border (outline pts 4,5 -> r11,r01)
        b.quad(P(o[4].x, o[4].y, +hd), r11, r01, P(o[5].x, o[5].y, +hd));
        // Left border (outline pts 5,6,7,0 -> r01,r00)
        b.tri(P(o[5].x, o[5].y, +hd), r01, P(o[6].x, o[6].y, +hd));
        b.tri(P(o[6].x, o[6].y, +hd), r01, r00);
        b.tri(P(o[6].x, o[6].y, +hd), r00, P(o[7].x, o[7].y, +hd));
        b.tri(P(o[7].x, o[7].y, +hd), r00, P(o[0].x, o[0].y, +hd));

        // Recessed label panel (pushed back) + its 4 side walls.
        const float zr = +hd - labDep;
        // panel face (normal +Z)
        b.quad(P(lx0, ly0, zr), P(lx1, ly0, zr), P(lx1, ly1, zr), P(lx0, ly1, zr));
        // four inset walls (front rim -> recessed rim)
        b.quad(P(lx0, ly0, +hd), P(lx1, ly0, +hd), P(lx1, ly0, zr), P(lx0, ly0, zr)); // bottom
        b.quad(P(lx1, ly0, +hd), P(lx1, ly1, +hd), P(lx1, ly1, zr), P(lx1, ly0, zr)); // right
        b.quad(P(lx1, ly1, +hd), P(lx0, ly1, +hd), P(lx0, ly1, zr), P(lx1, ly1, zr)); // top
        b.quad(P(lx0, ly1, +hd), P(lx0, ly0, +hd), P(lx0, ly0, zr), P(lx0, ly1, zr)); // left
    }

    // ---- Pass 3: BACK finger-grip ridges (V-grooves across lower-rear) ----
    {
        const int   N   = 4;
        const float gy0 = -hh + 0.10f * H;
        const float gy1 = -hh + 0.46f * H;
        const float gw  = (gy1 - gy0) / static_cast<float>(N);
        const float gx0 = -hw + taper + 0.06f * W;
        const float gx1 =  hw - taper - 0.06f * W;
        const float depth = 0.30f * D;   // groove depth (toward +Z, into body)
        for (int i = 0; i < N; ++i) {
            const float y0 = gy0 + i * gw;
            const float y1 = y0 + gw * 0.7f;
            const float ym = (y0 + y1) * 0.5f;
            // V-groove: two angled quads meeting at the recessed centre line.
            const float zb = -hd;            // back surface
            const float zr = -hd + depth;    // recessed valley
            b.quad(P(gx0, y0, zb), P(gx1, y0, zb), P(gx1, ym, zr), P(gx0, ym, zr));
            b.quad(P(gx0, ym, zr), P(gx1, ym, zr), P(gx1, y1, zb), P(gx0, y1, zb));
        }
    }

    // ---- Pass 4: BOTTOM connector notch (inset slot + guide tabs) ----
    {
        const float sx    = hw * 0.66f;       // slot half-width
        const float sUp   = -hh + 0.16f * H;  // slot ceiling (inside body)
        const float sZin  = +hd * 0.55f;      // slot front lip
        const float sZout = -hd * 0.55f;      // slot back lip
        const float yb    = -hh;
        // Slot recess: a downward-opening box. Ceiling + two side walls.
        // ceiling (normal -Y)
        b.quad(P(-sx, sUp, sZout), P(sx, sUp, sZout), P(sx, sUp, sZin), P(-sx, sUp, sZin));
        // left wall
        b.quad(P(-sx, yb, sZin), P(-sx, yb, sZout), P(-sx, sUp, sZout), P(-sx, sUp, sZin));
        // right wall
        b.quad(P(sx, yb, sZout), P(sx, yb, sZin), P(sx, sUp, sZin), P(sx, sUp, sZout));
        // front + back lips of the slot mouth (thin bands)
        b.quad(P(-sx, yb, sZin), P(sx, yb, sZin), P(sx, sUp, sZin), P(-sx, sUp, sZin));
        b.quad(P(sx, yb, sZout), P(-sx, yb, sZout), P(-sx, sUp, sZout), P(sx, sUp, sZout));
    }
}

} // namespace n64xr::holo
