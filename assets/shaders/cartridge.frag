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
    float ndv = clamp(dot(N, V), 0.0, 1.0);

    // (1) barycentric wireframe — thin, fwidth-AA.
    vec3  d    = fwidth(vBary);
    vec3  aa   = smoothstep(vec3(0.0), d * 1.1, vBary);
    float wire = 1.0 - min(min(aa.x, aa.y), aa.z);

    // (2) fresnel rim — only grazing angles light up.
    float fres = pow(1.0 - ndv, 3.0);

    // (3) one travelling phosphor sweep up object-Y — the hero glow.
    float sweep = fract(vObjHeight * 1.15 - t * u.timeParams.z);
    float band  = smoothstep(0.90, 1.0, sweep);

    // (4) gentle flicker — never strobing.
    float flicker = 0.94 + 0.06 * sin(t * 6.0) * sin(t * 11.0);
    float noise   = hash21(floor(gl_FragCoord.xy * 0.5) + floor(t * 20.0));
    flicker *= 1.0 - 0.03 * noise;

    // ---- compose; values kept mostly < 1 so ACES preserves the COLOUR
    //      instead of clipping everything to white. Only the sweep band and
    //      the rim cross 1.0 and get to bloom. ----
    vec3 col = vec3(0.0);

    // faint translucent face: warm brass over a hint of navy (kept warm so
    // stacked translucency reads amber-gold, not lilac).
    col += mix(NAVY, BRASS, 0.55) * (0.07 + 0.08 * ndv);

    // brass wireframe edges (dim — the structure, not the spectacle).
    col += BRASS * wire * 0.50;

    // brass fresnel rim.
    col += BRASS * fres * 0.55;

    // phosphor sweep — the one element that truly glows + blooms.
    col += PHOSPH * band * 1.25;

    col *= flicker;

    // subtle warm chromatic fringe on the extreme rim only (amber, not blue).
    float fr = pow(1.0 - ndv, 6.0) * 0.30;
    col.r += fr;
    col.g += fr * 0.5;

    // alpha: faces nearly see-through; edges / rim / sweep carry the form.
    float alpha = clamp(0.07 + wire * 0.55 + fres * 0.40 + band * 0.85, 0.0, 1.0);

    outColor = vec4(col, alpha);
}
