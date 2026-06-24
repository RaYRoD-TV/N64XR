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

    // Emit a triangle with EXPLICIT per-vertex barycentric. The wireframe
    // shader lights a pixel where any barycentric coord -> 0; by adding +1 to
    // the coord OPPOSITE an edge we want hidden, that edge's interior never
    // dips to 0 and stays dark. This lets us draw ONLY real cartridge edges
    // and suppress the internal triangulation diagonals.
    void triB(const Vec3& a, const Vec3& b, const Vec3& c,
              const Vec3& ba, const Vec3& bb, const Vec3& bc) {
        Vec3 n = normalize(cross(sub(b, a), sub(c, a)));
        uint32_t base = static_cast<uint32_t>(v.size());
        v.push_back({ a.x, a.y, a.z, n.x, n.y, n.z, ba.x, ba.y, ba.z });
        v.push_back({ b.x, b.y, b.z, n.x, n.y, n.z, bb.x, bb.y, bb.z });
        v.push_back({ c.x, c.y, c.z, n.x, n.y, n.z, bc.x, bc.y, bc.z });
        idx.push_back(base);
        idx.push_back(base + 1);
        idx.push_back(base + 2);
    }

    // Standard triangle — all three edges drawn (one-hot barycentric).
    void tri(const Vec3& a, const Vec3& b, const Vec3& c) {
        triB(a, b, c, Vec3{1,0,0}, Vec3{0,1,0}, Vec3{0,0,1});
    }

    // CCW quad -> two tris, with the SHARED DIAGONAL (a-c) hidden so only the
    // four real quad edges light up.
    void quad(const Vec3& a, const Vec3& b, const Vec3& c, const Vec3& d) {
        triB(a, b, c, Vec3{1,1,0}, Vec3{0,1,0}, Vec3{0,1,1}); // hide a-c (opp b)
        triB(a, c, d, Vec3{1,0,1}, Vec3{0,1,1}, Vec3{0,0,1}); // hide a-c (opp d)
    }

    // Fan triangle (apex a, rim b-c) that draws ONLY its outer rim edge b-c;
    // the two spokes a-b and a-c are hidden. For polygon cap fans.
    void fanTri(const Vec3& a, const Vec3& b, const Vec3& c) {
        triB(a, b, c, Vec3{1,1,1}, Vec3{0,1,1}, Vec3{0,1,1});
    }
};

inline Vec3 P(float x, float y, float z) { return { x, y, z }; }

} // namespace

void BuildCartridge(std::vector<Vertex>& outVerts,
                    std::vector<uint32_t>& outIndices) {
    outVerts.clear();
    outIndices.clear();
    Builder b{ outVerts, outIndices };

    // --- Portrait proportions, like a real N64 cart ---
    const float W = 0.75f, H = 1.0f, D = 0.15f;
    const float hw = W * 0.5f, hh = H * 0.5f, hd = D * 0.5f;
    const float taper = 0.035f * W;   // per-side bottom inset
    const float br    = 0.03f;        // bottom corner round
    const float sY    = 0.26f;        // shoulder height (top of the vertical sides)

    // N64 silhouette (XY plane), CCW: flat bottom, vertical sides, rounded
    // shoulders rising to a wide gently-domed CROWN — the signature top.
    struct V2 { float x, y; };
    const std::array<V2, 14> o = { {
        { -hw + taper, -hh           },  //  0 bottom-left
        {  hw - taper, -hh           },  //  1 bottom-right
        {  hw,         -hh + br       },  //  2 bottom-right round
        {  hw,          sY            },  //  3 right side -> shoulder
        {  hw - 0.015f, sY + 0.07f    },  //  4 shoulder curve
        {  hw - 0.060f, hh - 0.050f   },  //  5 upper-right corner curve
        {  hw - 0.130f, hh - 0.005f   },  //  6 crown start (right)
        {  0.105f,      hh            },  //  7 crown right
        { -0.105f,      hh            },  //  8 crown left
        { -hw + 0.130f, hh - 0.005f   },  //  9 crown start (left)
        { -hw + 0.060f, hh - 0.050f   },  // 10 upper-left corner curve
        { -hw + 0.015f, sY + 0.07f    },  // 11 shoulder curve
        { -hw,          sY            },  // 12 left side
        { -hw,         -hh + br       },  // 13 left side bottom round
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
            // wind so the face normal points -Z (outward at the back); fan so
            // only the outline rim draws (spokes to centre stay hidden).
            b.fanTri(cen, P(p1.x, p1.y, -hd), P(p0.x, p0.y, -hd));
        }
    }

    // ---- Front cap (fan, normal +Z) — clean outline edge only ----
    {
        const Vec3 cenF = P(0.0f, 0.0f, +hd);
        for (size_t i = 0; i < o.size(); ++i) {
            const V2 p0 = o[i];
            const V2 p1 = o[(i + 1) % o.size()];
            b.fanTri(cenF, P(p0.x, p0.y, +hd), P(p1.x, p1.y, +hd));
        }
    }

    // ---- Recessed square label, upper-centre (the classic N64 label well) ----
    {
        const float lhw = 0.205f, lcy = 0.135f, lhh = 0.195f;
        const float labDep = 0.045f;
        const float lx0 = -lhw, lx1 = lhw, ly0 = lcy - lhh, ly1 = lcy + lhh;
        const float zr  = +hd - labDep;
        const Vec3 r00 = P(lx0, ly0, +hd), r10 = P(lx1, ly0, +hd);
        const Vec3 r11 = P(lx1, ly1, +hd), r01 = P(lx0, ly1, +hd);
        // recessed panel (normal +Z)
        b.quad(P(lx0, ly0, zr), P(lx1, ly0, zr), P(lx1, ly1, zr), P(lx0, ly1, zr));
        // four inset walls — their front rim draws the crisp label border
        b.quad(r00, r10, P(lx1, ly0, zr), P(lx0, ly0, zr)); // bottom
        b.quad(r10, r11, P(lx1, ly1, zr), P(lx1, ly0, zr)); // right
        b.quad(r11, r01, P(lx0, ly1, zr), P(lx1, ly1, zr)); // top
        b.quad(r01, r00, P(lx0, ly0, zr), P(lx0, ly1, zr)); // left
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
