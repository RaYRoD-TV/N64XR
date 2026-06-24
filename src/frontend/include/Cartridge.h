// ============================================================================
//  Cartridge.h — procedural N64 cartridge mesh for the hologram.
// ----------------------------------------------------------------------------
//  Emits a recognisable N64 cartridge silhouette as flat-shaded triangles:
//    * wider-than-tall body with a slight bottom taper
//    * chamfered top corners + top edge (the strongest recognition cue)
//    * recessed front LABEL well (inset ring + sunken panel)
//    * horizontal finger-grip ridges on the back
//    * bottom connector notch flanked by guide tabs
//
//  Every vertex carries a barycentric coord (1,0,0)/(0,1,0)/(0,0,1) per
//  triangle corner so the wireframe shader can fwidth-AA the edges with no
//  MSAA. Normals are hard (per-face) for a crisp facet read. Centred at the
//  origin, normalised so width = 1.0 unit.
// ============================================================================
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace n64xr::holo {

struct Vertex {
    float px, py, pz;   // object-space position
    float nx, ny, nz;   // object-space normal (hard / per-face)
    float bx, by, bz;   // barycentric stamp (crease-only when loaded from glTF)
};

// Build the procedural cartridge (default / fallback when no model is present).
void BuildCartridge(std::vector<Vertex>& outVerts,
                    std::vector<uint32_t>& outIndices);

// Load a .glb/.gltf cartridge model into the same Vertex format, with
// crease-only barycentric so the wireframe shader draws just the real edges.
// Auto-centres / orients / scales the mesh. Returns false on any failure so
// the caller can fall back to BuildCartridge.
bool LoadCartridgeMesh(const std::string& path,
                       std::vector<Vertex>& outVerts,
                       std::vector<uint32_t>& outIndices);

} // namespace n64xr::holo
