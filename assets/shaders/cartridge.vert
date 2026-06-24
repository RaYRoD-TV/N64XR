#version 450

layout(location = 0) in vec3 inPos;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec3 inBary;

layout(set = 0, binding = 0) uniform UBO {
    mat4 model;
    mat4 view;
    mat4 proj;
    mat4 normalMat;
    vec4 camPos;       // .xyz world-space eye
    vec4 timeParams;   // x=time, y=glitchAmp, z=sweepSpeed, w=unused
} u;

layout(location = 0) out vec3 vBary;
layout(location = 1) out vec3 vWorldPos;
layout(location = 2) out vec3 vWorldNrm;
layout(location = 3) out float vObjHeight;

float hash11(float p) {
    p = fract(p * 0.1031);
    p *= p + 33.33;
    p *= p + p;
    return fract(p);
}

void main() {
    float t = u.timeParams.x;

    // Rare, quantized vertex glitch along the normal — mostly zero.
    float band   = step(0.97, fract(sin(floor(t * 8.0)) * 43758.5453));
    float jitter = (hash11(inPos.y * 50.0 + floor(t * 8.0)) - 0.5);
    vec3  glitched = inPos + inNormal * jitter * u.timeParams.y * band;

    vec4 world = u.model * vec4(glitched, 1.0);
    vWorldPos  = world.xyz;
    vWorldNrm  = normalize(mat3(u.normalMat) * inNormal);
    vBary      = inBary;
    vObjHeight = inPos.y;   // stable (pre-glitch) sweep axis

    gl_Position = u.proj * u.view * world;
}
