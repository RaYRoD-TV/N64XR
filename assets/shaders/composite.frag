#version 450

// Paints an enclosed cyan-grid ROOM interior (floor + ceiling + side walls +
// back wall, in perspective — the camera sits inside the box), adds the
// holographic cartridge scene + bloom, tonemaps. Runs inside the swapchain
// pass before ImGui; output is opaque so ImGui blends over it.
layout(location = 0) in vec2 uv;
layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform sampler2D sceneTex;  // HDR cartridge
layout(set = 0, binding = 1) uniform sampler2D bloomTex;  // bloom mip[0]

layout(push_constant) uniform PC {
    vec2  invResolution;   // (1/w, 1/h)
    float time;
    float bloomStrength;
} pc;

const vec3 CYAN = vec3(0.157, 0.902, 0.941); // #28E6F0

float hash21(vec2 p) {
    p = fract(p * vec2(123.34, 456.21));
    p += dot(p, p + 45.32);
    return fract(p.x * p.y);
}

void main() {
    // ---- view ray from inside the room, looking down -Z ----
    vec2  ndc    = uv * 2.0 - 1.0;
    float aspect = pc.invResolution.y / max(pc.invResolution.x, 1e-6); // w/h
    vec3  ro = vec3(0.0, 0.0, 0.0);
    vec3  rd = normalize(vec3(ndc.x * aspect, -ndc.y, -1.55));

    // ---- the room box (camera near the front, back wall far down -Z) ----
    const float HW = 2.30, HH = 1.32, DEPTH = 7.2;
    const vec3  boxC = vec3(0.0, 0.0, -DEPTH * 0.5);
    const vec3  hw   = vec3(HW, HH, DEPTH * 0.5);

    vec3 oc  = ro - boxC;
    vec3 inv = 1.0 / rd;
    vec3 t1  = (-hw - oc) * inv;
    vec3 t2  = ( hw - oc) * inv;
    vec3 tm  = max(t1, t2);
    float te = min(min(tm.x, tm.y), tm.z);     // exit = the wall we see
    te = max(te, 0.001);

    vec3 hit = ro + rd * te;
    vec3 hl  = hit - boxC;

    // tangent coords of whichever face we hit (so the grid lies in-plane)
    vec3 an = abs(hl) / hw;
    vec2 tang;
    if (an.x >= an.y && an.x >= an.z)      tang = hl.zy;  // side wall
    else if (an.y >= an.z)                 tang = hl.xz;  // floor / ceiling
    else                                   tang = hl.xy;  // back wall

    // ---- grid lines, fwidth-AA ----
    const float cell = 0.58;
    vec2  g     = tang / cell;
    vec2  gd    = abs(fract(g) - 0.5);
    vec2  gwid  = fwidth(g);
    vec2  lines = smoothstep(vec2(0.5), vec2(0.5) - gwid * 2.2, gd);
    float grid  = max(lines.x, lines.y);

    // ---- shade the room ----
    float fade = exp(-te * 0.165);                 // farther walls dim -> depth
    vec3  room = vec3(0.008, 0.024, 0.045);        // dark wall base
    room += CYAN * grid * fade * 0.95;             // glowing grid lines
    room += CYAN * 0.035 * fade;                   // faint wall ambient
    room += CYAN * smoothstep(0.55, 0.0, length(ndc)) * 0.09; // back vanishing glow

    // vignette + grain
    float vig = smoothstep(1.30, 0.30, length(ndc));
    room *= mix(0.45, 1.0, vig);
    room += (hash21(uv * vec2(1920.0, 1080.0)) - 0.5) * 0.012;

    // ---- composite: room + cartridge scene + bloom ----
    vec3 scene = texture(sceneTex, uv).rgb;
    vec3 bloom = texture(bloomTex, uv).rgb * pc.bloomStrength;
    vec3 hdr   = room + scene + bloom;

    // ACES-ish tonemap.
    vec3 mapped = clamp((hdr * (2.51 * hdr + 0.03)) / (hdr * (2.43 * hdr + 0.59) + 0.14),
                        0.0, 1.0);
    outColor = vec4(mapped, 1.0);
}
