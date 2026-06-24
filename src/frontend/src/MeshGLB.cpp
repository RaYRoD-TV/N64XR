// ============================================================================
//  MeshGLB.cpp — load a .glb/.gltf cartridge model into the hologram's
//  Vertex format (position + hard face normal + CREASE-only barycentric).
// ----------------------------------------------------------------------------
//  A real cartridge model is densely triangulated, so lighting every triangle
//  edge would be a crosshatch mess. Instead we keep only edges that are a
//  real crease (face-angle past a threshold) or a boundary/silhouette edge,
//  and feed the wireframe shader barycentric coords that hide the rest — the
//  same +1-to-the-opposite-coord trick the procedural mesh uses.
//
//  The mesh is auto-normalised: centred, axis-oriented (smallest extent -> Z
//  depth, largest -> Y height), and scaled so its height == 1.0 to match the
//  procedural cart. tinygltf is compiled here (geometry only, no image libs).
// ============================================================================

#include "Cartridge.h"
#include "HoloMath.h"

#define TINYGLTF_IMPLEMENTATION
#define TINYGLTF_NO_STB_IMAGE
#define TINYGLTF_NO_STB_IMAGE_WRITE
#define TINYGLTF_NO_EXTERNAL_IMAGE
#include <tiny_gltf.h>

#include <spdlog/spdlog.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <map>
#include <utility>
#include <vector>

namespace n64xr::holo {
namespace {

bool endsWith(const std::string& s, const char* suf) {
    const std::string x(suf);
    return s.size() >= x.size() && s.compare(s.size() - x.size(), x.size(), x) == 0;
}

struct Tri { uint32_t a, b, c; };

} // namespace

bool LoadCartridgeMesh(const std::string& path,
                       std::vector<Vertex>& outV,
                       std::vector<uint32_t>& outI) {
    tinygltf::TinyGLTF loader;
    tinygltf::Model m;
    std::string err, warn;
    const bool ok = endsWith(path, ".glb")
        ? loader.LoadBinaryFromFile(&m, &err, &warn, path)
        : loader.LoadASCIIFromFile(&m, &err, &warn, path);
    if (!warn.empty()) spdlog::debug("[glTF] {}", warn);
    if (!ok || !err.empty()) {
        spdlog::warn("[glTF] load failed '{}': {}", path, err);
        return false;
    }

    // ---- gather an indexed triangle soup across all TRIANGLES primitives ----
    std::vector<std::array<float, 3>> pos;
    std::vector<Tri> tris;

    for (const auto& mesh : m.meshes) {
        for (const auto& prim : mesh.primitives) {
            if (prim.mode != TINYGLTF_MODE_TRIANGLES && prim.mode != -1) continue;
            auto it = prim.attributes.find("POSITION");
            if (it == prim.attributes.end()) continue;

            const tinygltf::Accessor&   pa = m.accessors[it->second];
            const tinygltf::BufferView& pv = m.bufferViews[pa.bufferView];
            const tinygltf::Buffer&     pb = m.buffers[pv.buffer];
            if (pa.type != TINYGLTF_TYPE_VEC3 ||
                pa.componentType != TINYGLTF_COMPONENT_TYPE_FLOAT) continue;

            const int    declared = pa.ByteStride(pv);
            const size_t pStride   = declared > 0 ? size_t(declared) : sizeof(float) * 3;
            const uint8_t* pBase   = pb.data.data() + pv.byteOffset + pa.byteOffset;
            const uint32_t baseVert = static_cast<uint32_t>(pos.size());

            for (size_t i = 0; i < pa.count; ++i) {
                const float* f = reinterpret_cast<const float*>(pBase + i * pStride);
                pos.push_back({ f[0], f[1], f[2] });
            }

            if (prim.indices < 0) {
                for (size_t i = 0; i + 2 < pa.count; i += 3)
                    tris.push_back({ baseVert + uint32_t(i),
                                     baseVert + uint32_t(i + 1),
                                     baseVert + uint32_t(i + 2) });
            } else {
                const tinygltf::Accessor&   ia = m.accessors[prim.indices];
                const tinygltf::BufferView& iv = m.bufferViews[ia.bufferView];
                const tinygltf::Buffer&     ib = m.buffers[iv.buffer];
                const uint8_t* iBase = ib.data.data() + iv.byteOffset + ia.byteOffset;
                auto readIdx = [&](size_t i) -> uint32_t {
                    switch (ia.componentType) {
                        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
                            return reinterpret_cast<const uint32_t*>(iBase)[i];
                        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
                            return reinterpret_cast<const uint16_t*>(iBase)[i];
                        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
                            return reinterpret_cast<const uint8_t*>(iBase)[i];
                        default: return 0;
                    }
                };
                for (size_t i = 0; i + 2 < ia.count; i += 3)
                    tris.push_back({ baseVert + readIdx(i),
                                     baseVert + readIdx(i + 1),
                                     baseVert + readIdx(i + 2) });
            }
        }
    }

    if (pos.empty() || tris.empty()) {
        spdlog::warn("[glTF] '{}' had no triangle geometry.", path);
        return false;
    }

    // ---- normalise: centre, axis-orient, scale height -> 1.0 ----
    Vec3 lo{ 1e30f, 1e30f, 1e30f }, hi{ -1e30f, -1e30f, -1e30f };
    for (const auto& p : pos) {
        lo.x = std::min(lo.x, p[0]); lo.y = std::min(lo.y, p[1]); lo.z = std::min(lo.z, p[2]);
        hi.x = std::max(hi.x, p[0]); hi.y = std::max(hi.y, p[1]); hi.z = std::max(hi.z, p[2]);
    }
    const std::array<float, 3> cen = { (lo.x + hi.x) * 0.5f, (lo.y + hi.y) * 0.5f, (lo.z + hi.z) * 0.5f };
    const std::array<float, 3> ext = { hi.x - lo.x, hi.y - lo.y, hi.z - lo.z };

    int aMin = 0; if (ext[1] < ext[aMin]) aMin = 1; if (ext[2] < ext[aMin]) aMin = 2;
    int aMax = 0; if (ext[1] > ext[aMax]) aMax = 1; if (ext[2] > ext[aMax]) aMax = 2;
    int aMid = 3 - aMin - aMax;
    if (aMid < 0 || aMid > 2 || aMin == aMax) { aMin = 2; aMax = 1; aMid = 0; } // fallback to XYZ

    const float scale = (ext[aMax] > 1e-6f) ? (1.0f / ext[aMax]) : 1.0f;
    std::vector<Vec3> P(pos.size());
    for (size_t i = 0; i < pos.size(); ++i) {
        const std::array<float, 3>& s = pos[i];
        const float cx = s[aMid] - cen[aMid];   // width  -> X
        const float cy = s[aMax] - cen[aMax];   // height -> Y
        const float cz = s[aMin] - cen[aMin];   // depth  -> Z
        P[i] = Vec3{ cx * scale, cy * scale, cz * scale };
    }

    // ---- orientation correction --------------------------------------------
    //  Auto-orient can't tell which way is "up"/"front", so dial it here if a
    //  model loads rotated. Degrees, applied X then Y then Z. (Default: a 90°
    //  roll to stand a landscape-authored cart upright.)
    {
        const float kRotX = 0.0f;
        const float kRotY = 0.0f;
        const float kRotZ = 90.0f;
        if (kRotX != 0.0f || kRotY != 0.0f || kRotZ != 0.0f) {
            const float dr = 3.14159265f / 180.0f;
            const Mat4 R = mul(mul(rotateZ(kRotZ * dr), rotateY(kRotY * dr)), rotateX(kRotX * dr));
            for (auto& p : P) {
                const float x = p.x, y = p.y, z = p.z;
                p.x = R.m[0][0]*x + R.m[1][0]*y + R.m[2][0]*z;
                p.y = R.m[0][1]*x + R.m[1][1]*y + R.m[2][1]*z;
                p.z = R.m[0][2]*x + R.m[1][2]*y + R.m[2][2]*z;
            }
        }
    }

    // ---- face normals ----
    std::vector<Vec3> fn(tris.size());
    for (size_t t = 0; t < tris.size(); ++t) {
        const Vec3& a = P[tris[t].a]; const Vec3& b = P[tris[t].b]; const Vec3& c = P[tris[t].c];
        fn[t] = normalize(cross(sub(b, a), sub(c, a)));
    }

    // ---- edge adjacency + crease test ----
    struct EdgeInfo { int t0 = -1, t1 = -1; };
    std::map<std::pair<uint32_t, uint32_t>, EdgeInfo> edges;
    auto ekey = [](uint32_t x, uint32_t y) {
        return x < y ? std::make_pair(x, y) : std::make_pair(y, x);
    };
    auto addEdge = [&](uint32_t x, uint32_t y, int t) {
        EdgeInfo& e = edges[ekey(x, y)];
        if (e.t0 < 0) e.t0 = t; else if (e.t1 < 0) e.t1 = t;
    };
    for (size_t t = 0; t < tris.size(); ++t) {
        addEdge(tris[t].a, tris[t].b, int(t));
        addEdge(tris[t].b, tris[t].c, int(t));
        addEdge(tris[t].c, tris[t].a, int(t));
    }
    const float creaseCos = std::cos(20.0f * 3.14159265f / 180.0f); // crease if angle > 20 deg
    auto isCrease = [&](uint32_t x, uint32_t y) -> bool {
        auto it = edges.find(ekey(x, y));
        if (it == edges.end()) return true;
        const EdgeInfo& e = it->second;
        if (e.t1 < 0) return true;                    // boundary / silhouette
        return dot(fn[e.t0], fn[e.t1]) < creaseCos;   // sharp dihedral
    };

    // ---- de-index, hiding every non-crease edge from the wireframe shader ----
    outV.clear(); outI.clear();
    outV.reserve(tris.size() * 3);
    outI.reserve(tris.size() * 3);
    for (size_t t = 0; t < tris.size(); ++t) {
        const Tri& T = tris[t];
        Vec3 ba{ 1, 0, 0 }, bb{ 0, 1, 0 }, bc{ 0, 0, 1 };
        if (!isCrease(T.a, T.b)) { ba.z += 1.0f; bb.z += 1.0f; } // edge a-b opposite c (z)
        if (!isCrease(T.b, T.c)) { bb.x += 1.0f; bc.x += 1.0f; } // edge b-c opposite a (x)
        if (!isCrease(T.c, T.a)) { bc.y += 1.0f; ba.y += 1.0f; } // edge c-a opposite b (y)

        const Vec3& n = fn[t];
        const uint32_t base = static_cast<uint32_t>(outV.size());
        auto push = [&](const Vec3& p, const Vec3& bary) {
            outV.push_back({ p.x, p.y, p.z, n.x, n.y, n.z, bary.x, bary.y, bary.z });
        };
        push(P[T.a], ba); push(P[T.b], bb); push(P[T.c], bc);
        outI.push_back(base); outI.push_back(base + 1); outI.push_back(base + 2);
    }

    spdlog::info("[glTF] cartridge loaded: {} tris from '{}'.", tris.size(), path);
    return true;
}

} // namespace n64xr::holo
