#version 450

// Paints the deep-space CABINET background, adds bloom, tonemaps. Runs inside
// the swapchain pass before ImGui. Output is opaque (alpha 1) so ImGui blends
// over it normally.
layout(location = 0) in vec2 uv;
layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform sampler2D sceneTex;  // HDR cartridge
layout(set = 0, binding = 1) uniform sampler2D bloomTex;  // bloom mip[0]

layout(push_constant) uniform PC {
    vec2  invResolution;
    float time;
    float bloomStrength;
} pc;

// Palette.
const vec3 DEEP_NAVY = vec3(0.043, 0.063, 0.102); // #0B101A
const vec3 NAVY_TOP  = vec3(0.020, 0.030, 0.060);
const vec3 PHOSPH    = vec3(0.486, 0.890, 0.545);
const vec3 BRASS     = vec3(0.788, 0.604, 0.239);

float hash21(vec2 p) {
    p = fract(p * vec2(123.34, 456.21));
    p += dot(p, p + 45.32);
    return fract(p.x * p.y);
}

// Soft starfield: sparse points on a hashed grid, two parallax layers.
float starLayer(vec2 uvp, float density, float bright) {
    vec2 g  = floor(uvp * density);
    vec2 f  = fract(uvp * density);
    float h = hash21(g);
    // only a few cells host a star
    float present = step(0.985, h);
    vec2  c = vec2(hash21(g + 1.3), hash21(g + 7.7));
    float d = length(f - c);
    float star = present * smoothstep(0.06, 0.0, d);
    // gentle twinkle
    star *= 0.6 + 0.4 * sin(h * 100.0);
    return star * bright;
}

void main() {
    // ---- deep-navy vertical gradient ----
    vec3 bg = mix(DEEP_NAVY, NAVY_TOP, uv.y);

    // ---- parallax starfield (two layers, faint) ----
    vec2 p = uv;
    p.x *= (pc.invResolution.y / max(pc.invResolution.x, 1e-6)); // aspect-correct
    float stars = starLayer(p, 16.0, 0.55) + starLayer(p * 1.7 + 3.1, 28.0, 0.30);
    bg += vec3(0.7, 0.78, 0.9) * stars;

    // ---- soft vignette ----
    vec2 vc = uv - 0.5;
    float vig = smoothstep(0.95, 0.35, length(vc));
    bg *= mix(0.55, 1.0, vig);

    // ---- grain ----
    float grain = hash21(uv * vec2(1920.0, 1080.0)) - 0.5;
    bg += grain * 0.015;

    // ---- composite: background + scene + bloom ----
    vec3 scene = texture(sceneTex, uv).rgb;
    vec3 bloom = texture(bloomTex, uv).rgb * pc.bloomStrength;
    vec3 hdr   = bg + scene + bloom;

    // ACES-ish tonemap.
    vec3 x = hdr;
    vec3 mapped = clamp((x * (2.51 * x + 0.03)) / (x * (2.43 * x + 0.59) + 0.14), 0.0, 1.0);

    outColor = vec4(mapped, 1.0);
}
