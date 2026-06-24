// ============================================================================
//  HoloMath.h — tiny dependency-free column-major mat4 math for HoloStage.
// ----------------------------------------------------------------------------
//  Just enough for a spinning cartridge: perspective (Vulkan clip space:
//  Y-flip + [0,1] depth), lookAt (right-handed), rotateY, translate, and
//  matrix*matrix / matrix*vec4. Column-major to match GLSL's mat4 memory
//  layout, so a straight memcpy into a std140 UBO is correct.
// ============================================================================
#pragma once

#include <cmath>

namespace n64xr::holo {

struct Vec3 { float x = 0.0f, y = 0.0f, z = 0.0f; };
struct Vec4 { float x = 0.0f, y = 0.0f, z = 0.0f, w = 0.0f; };

// Column-major 4x4. m[col][row]. m[c] is column c (4 floats), matching GLSL.
struct Mat4 {
    float m[4][4] = {{0}};

    static Mat4 identity() {
        Mat4 r;
        r.m[0][0] = 1.0f; r.m[1][1] = 1.0f; r.m[2][2] = 1.0f; r.m[3][3] = 1.0f;
        return r;
    }
};

inline Vec3 sub(const Vec3& a, const Vec3& b) { return { a.x - b.x, a.y - b.y, a.z - b.z }; }
inline float dot(const Vec3& a, const Vec3& b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
inline Vec3 cross(const Vec3& a, const Vec3& b) {
    return { a.y * b.z - a.z * b.y,
             a.z * b.x - a.x * b.z,
             a.x * b.y - a.y * b.x };
}
inline Vec3 normalize(const Vec3& a) {
    float len = std::sqrt(dot(a, a));
    if (len < 1e-8f) return { 0.0f, 0.0f, 0.0f };
    float inv = 1.0f / len;
    return { a.x * inv, a.y * inv, a.z * inv };
}

// C = A * B (column-major; applies B first, then A to a column vector).
inline Mat4 mul(const Mat4& a, const Mat4& b) {
    Mat4 r;
    for (int c = 0; c < 4; ++c) {
        for (int row = 0; row < 4; ++row) {
            r.m[c][row] = a.m[0][row] * b.m[c][0] +
                          a.m[1][row] * b.m[c][1] +
                          a.m[2][row] * b.m[c][2] +
                          a.m[3][row] * b.m[c][3];
        }
    }
    return r;
}

// Vulkan-friendly right-handed perspective.
//   * depth range [0,1]   (NOT OpenGL's [-1,1])
//   * Y flipped           (Vulkan clip-space +Y is down)
// fovYRad = vertical field of view in radians.
inline Mat4 perspective(float fovYRad, float aspect, float znear, float zfar) {
    Mat4 r; // zero-initialised
    const float f = 1.0f / std::tan(fovYRad * 0.5f);
    r.m[0][0] = f / aspect;
    r.m[1][1] = -f;                              // Y-flip for Vulkan
    r.m[2][2] = zfar / (znear - zfar);           // [0,1] depth, RH
    r.m[2][3] = -1.0f;
    r.m[3][2] = (zfar * znear) / (znear - zfar);
    return r;
}

// Right-handed lookAt.
inline Mat4 lookAt(const Vec3& eye, const Vec3& center, const Vec3& up) {
    const Vec3 fwd = normalize(sub(center, eye));   // forward (-Z in view)
    const Vec3 s   = normalize(cross(fwd, up));      // right
    const Vec3 u   = cross(s, fwd);                  // true up

    Mat4 r = Mat4::identity();
    r.m[0][0] = s.x;   r.m[1][0] = s.y;   r.m[2][0] = s.z;
    r.m[0][1] = u.x;   r.m[1][1] = u.y;   r.m[2][1] = u.z;
    r.m[0][2] = -fwd.x; r.m[1][2] = -fwd.y; r.m[2][2] = -fwd.z;
    r.m[3][0] = -dot(s, eye);
    r.m[3][1] = -dot(u, eye);
    r.m[3][2] = dot(fwd, eye);
    return r;
}

inline Mat4 rotateY(float angleRad) {
    Mat4 r = Mat4::identity();
    const float c = std::cos(angleRad);
    const float s = std::sin(angleRad);
    r.m[0][0] = c;   r.m[2][0] = s;
    r.m[0][2] = -s;  r.m[2][2] = c;
    return r;
}

inline Mat4 rotateX(float angleRad) {
    Mat4 r = Mat4::identity();
    const float c = std::cos(angleRad);
    const float s = std::sin(angleRad);
    r.m[1][1] = c;   r.m[2][1] = -s;
    r.m[1][2] = s;   r.m[2][2] = c;
    return r;
}

inline Mat4 rotateZ(float angleRad) {
    Mat4 r = Mat4::identity();
    const float c = std::cos(angleRad);
    const float s = std::sin(angleRad);
    r.m[0][0] = c;   r.m[1][0] = -s;
    r.m[0][1] = s;   r.m[1][1] = c;
    return r;
}

inline Mat4 translate(float x, float y, float z) {
    Mat4 r = Mat4::identity();
    r.m[3][0] = x; r.m[3][1] = y; r.m[3][2] = z;
    return r;
}

} // namespace n64xr::holo
