#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inUV;

layout(location = 0) out vec3 vWorldNormal;
layout(location = 1) out vec3 vWorldPos;
layout(location = 2) out vec2 vUV;

layout(push_constant) uniform PushConstants {
    mat4 model;
    mat4 view;
    mat4 proj;
} pc;

void main() {
    vec4 worldPos = pc.model * vec4(inPosition, 1.0);
    mat3 normalMatrix = transpose(inverse(mat3(pc.model)));

    vWorldPos = worldPos.xyz;
    vWorldNormal = normalize(normalMatrix * inNormal);
    vUV = inUV;

    gl_Position = pc.proj * pc.view * worldPos;
}
