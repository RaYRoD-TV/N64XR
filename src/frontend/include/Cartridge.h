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
#include <vector>

namespace n64xr::holo {

struct Vertex {
    float px, py, pz;   // object-space position
    float nx, ny, nz;   // object-space normal (hard / per-face)
    float bx, by, bz;   // barycentric stamp
};

// Build the cartridge. Appends into the provided vectors (cleared first).
void BuildCartridge(std::vector<Vertex>& outVerts,
                    std::vector<uint32_t>& outIndices);

} // namespace n64xr::holo
