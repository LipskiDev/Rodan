#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inUV;
layout(location = 3) in vec4 inTangent;

layout(location = 0) out vec3 vWorldNormal;
layout(location = 1) out vec3 vWorldPos;
layout(location = 2) out vec2 vUV;
layout(location = 3) out vec3 vTangent;
layout(location = 4) out vec3 vBitangent;
layout(location = 5) out vec3 vNormal;

layout(set = 1, binding = 1) uniform FrameData {
  mat4 view;
  mat4 proj;

  mat4 lightViewProj;
  vec4 lightDirection;
  vec4 lightColor;
  float lightIntensity;
  float shadowsEnabled;
  int showMode;
} u_Frame;

layout(push_constant) uniform PushConstants {
    mat4 model;              // offset 0,   size 64
    vec4 baseColorFactor;    // offset 64,  size 16

    float metallicFactor;    // offset 80
    float roughnessFactor;   // offset 84
    float alphaCutoff;       // offset 88

    int showMode;            // offset 92
    int hasMaterial;         // offset 96
    int alphaMode;           // offset 100
    int hasTangents;         // offset 104
} pc;

void main() {
    vec4 worldPos = pc.model * vec4(inPosition, 1.0);

    mat3 normalMatrix = transpose(inverse(mat3(pc.model)));

    vec3 N = normalize(mat3(normalMatrix) * inNormal);
    vec3 T = normalize(mat3(normalMatrix) * inTangent.xyz);

    T = normalize(T - dot(T, N) * N);

    vec3 B = normalize(cross(N, T)) * inTangent.w;

    vTangent = T;
    vBitangent = B;
    vNormal = N;
    vWorldNormal = N;
    vWorldPos = worldPos.xyz;
    vUV = inUV;

    gl_Position = u_Frame.proj * u_Frame.view * worldPos;
}
