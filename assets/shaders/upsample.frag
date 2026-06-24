#version 450

// 3x3 tent upsample; additively blended onto the larger mip by the pipeline.
layout(location = 0) in vec2 uv;
layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform sampler2D src;

layout(push_constant) uniform PC {
    vec2  texelSize;   // SOURCE (smaller) mip texel size
    float threshold;
    float knee;
    float filterRadius;
} pc;

vec3 T(vec2 o) { return texture(src, uv + o).rgb; }

void main() {
    vec2 dd = pc.texelSize * pc.filterRadius;
    vec3 s =
          T(vec2(-dd.x,  dd.y)) + T(vec2(0.0,  dd.y)) * 2.0 + T(vec2(dd.x,  dd.y))
        + T(vec2(-dd.x,  0.0)) * 2.0 + T(vec2(0.0,  0.0)) * 4.0 + T(vec2(dd.x,  0.0)) * 2.0
        + T(vec2(-dd.x, -dd.y)) + T(vec2(0.0, -dd.y)) * 2.0 + T(vec2(dd.x, -dd.y));
    outColor = vec4(s * (1.0 / 16.0), 1.0);
}
