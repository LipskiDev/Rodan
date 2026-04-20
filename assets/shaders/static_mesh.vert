#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inUV;
layout(location = 3) in vec4 inTangent;

layout(location = 0) out vec3 vWorldNormal;
layout(location = 1) out vec3 vWorldPos;
layout(location = 2) out vec2 vUV;
layout(location = 3) out mat3 vTBN;

layout(push_constant) uniform PushConstants {
    mat4 model;
    mat4 view;
    mat4 proj;

    vec4 baseColorFactor;
    float metallicFactor;
    float roughnessFactor;
    int hasMaterial;
} pc;

void main() {
    vec4 worldPos = pc.model * vec4(inPosition, 1.0);

    mat3 normalMatrix = transpose(inverse(mat3(pc.model)));

    vec3 N = normalize(mat3(normalMatrix) * inNormal);
    vec3 T = normalize(mat3(normalMatrix) * inTangent.xyz);

    T = normalize(T - dot(T, N) * N);

    vec3 B = cross(N, T) * inTangent.w;

    vTBN = mat3(T, B, N);
    vWorldNormal = N;
    vWorldPos = worldPos.xyz;
    vUV = inUV;

    gl_Position = pc.proj * pc.view * worldPos;
}
