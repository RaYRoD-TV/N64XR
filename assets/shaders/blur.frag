#version 450

// 13-tap partial-Karis downsample (kills fireflies, anti-aliases bright-pass).
layout(location = 0) in vec2 uv;
layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform sampler2D src;

layout(push_constant) uniform PC {
    vec2  texelSize;   // SOURCE mip texel size
    float threshold;
    float knee;
    float filterRadius;
} pc;

vec3 T(vec2 o) { return texture(src, uv + o * pc.texelSize).rgb; }

void main() {
    vec3 a = T(vec2(-2,  2)), b = T(vec2( 0,  2)), c = T(vec2( 2,  2));
    vec3 d = T(vec2(-2,  0)), e = T(vec2( 0,  0)), f = T(vec2( 2,  0));
    vec3 g = T(vec2(-2, -2)), h = T(vec2( 0, -2)), i = T(vec2( 2, -2));
    vec3 j = T(vec2(-1,  1)), k = T(vec2( 1,  1));
    vec3 l = T(vec2(-1, -1)), m = T(vec2( 1, -1));

    vec3 col = e * 0.125;
    col += (a + c + g + i) * 0.03125;
    col += (b + d + f + h) * 0.0625;
    col += (j + k + l + m) * 0.125;
    outColor = vec4(col, 1.0);
}
