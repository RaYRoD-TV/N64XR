#version 450

layout(location = 0) in vec2 uv;
layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform sampler2D src;

layout(push_constant) uniform PC {
    vec2  texelSize;
    float threshold;
    float knee;
    float filterRadius;
} pc;

void main() {
    vec3  c  = texture(src, uv).rgb;
    float br = max(c.r, max(c.g, c.b));

    // Sousa quadratic soft-knee — no hard cutoff, no flicker.
    float k    = pc.knee + 1e-4;
    float soft = clamp(br - pc.threshold + k, 0.0, 2.0 * k);
    soft = soft * soft / (4.0 * k);
    float contrib = max(soft, br - pc.threshold) / max(br, 1e-4);

    outColor = vec4(c * contrib, 1.0);
}
