#version 450

layout(location = 0) in vec3 vBary;
layout(location = 1) in vec3 vWorldPos;
layout(location = 2) in vec3 vWorldNrm;
layout(location = 3) in float vObjHeight;

layout(set = 0, binding = 0) uniform UBO {
    mat4 model;
    mat4 view;
    mat4 proj;
    mat4 normalMat;
    vec4 camPos;
    vec4 timeParams;   // x=time, y=glitchAmp, z=sweepSpeed, w=unused
} u;

layout(location = 0) out vec4 outColor;

// Palette (linear-ish).
const vec3 BRASS  = vec3(0.788, 0.604, 0.239); // #C99A3D
const vec3 PHOSPH = vec3(0.486, 0.890, 0.545); // #7CE38B
const vec3 NAVY   = vec3(0.043, 0.063, 0.137);

float hash21(vec2 p) {
    p = fract(p * vec2(123.34, 456.21));
    p += dot(p, p + 45.32);
    return fract(p.x * p.y);
}

void main() {
    float t   = u.timeParams.x;
    vec3  N   = normalize(vWorldNrm);
    vec3  V   = normalize(u.camPos.xyz - vWorldPos);
    float ndv = max(dot(N, V), 0.0);

    // (1) barycentric wireframe, fwidth-AA.
    vec3  d = fwidth(vBary);
    vec3  a = smoothstep(vec3(0.0), d * 1.5, vBary);
    float wire = 1.0 - min(min(a.x, a.y), a.z);

    // (2) fresnel rim — high exponent so only grazing angles bloom.
    float fres = pow(1.0 - ndv, 3.5);

    // (3) single travelling phosphor scanline sweep up object-Y.
    float sweep = fract(vObjHeight * 1.5 - t * u.timeParams.z);
    float bandG = smoothstep(0.96, 1.0, sweep) + smoothstep(0.96, 1.0, 1.0 - sweep);

    // fine screen-space raster lines, very subtle.
    float raster = 0.5 + 0.5 * sin(gl_FragCoord.y * 1.3);
    raster = mix(1.0, raster, 0.10);

    // (5) gentle flicker + scanline noise, never strobing.
    float flicker = 0.92 + 0.08 * sin(t * 7.0) * sin(t * 13.0);
    float noise   = hash21(floor(gl_FragCoord.xy * 0.5) + floor(t * 24.0));
    flicker *= 1.0 - 0.04 * noise;

    float faceFill = 0.10 + 0.06 * ndv;        // (4) translucent interior
    float edges    = wire * 1.4 + fres * 1.1;
    float glow     = bandG * 1.6;

    float intensity = (faceFill + edges + glow) * flicker * raster;

    vec3 col = NAVY;
    col = mix(col, BRASS,  clamp(fres + wire * 0.5, 0.0, 1.0));
    col = mix(col, PHOSPH, clamp(glow + wire * 0.4, 0.0, 1.0));
    col *= intensity;

    // chromatic fringe along the rim only.
    float fr = pow(1.0 - ndv, 5.0) * 0.5;
    col.r += fr * PHOSPH.r * 0.4;
    col.b += fr * 0.6;

    // soft knee so highlights roll instead of clipping (HDR target tolerates >1).
    col = col / (col + vec3(0.7)) * 1.7;

    float alpha = clamp(faceFill * 0.4 + edges * 0.8 + glow, 0.0, 1.0);
    outColor = vec4(col, alpha);
}
