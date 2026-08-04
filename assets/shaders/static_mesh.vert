#version 450
#extension GL_ARB_shader_draw_parameters : require

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
layout(location = 6) flat out uint vDrawIndex;

const int MAX_SHADOW_CASCADES = 8;

struct CascadeData {
  mat4 lightViewProj;
  vec4 splitData;
};

layout(std140, set = 1, binding = 1) uniform FrameData {
  mat4 view;
  mat4 proj;

  CascadeData cascades[MAX_SHADOW_CASCADES];

  uint cascadeCount;
  uint _cascadePad0;
  uint _cascadePad1;
  uint _cascadePad2;

  vec4 lightDirection;
  vec4 lightColor;

  vec2 viewportSize;

  float lightIntensity;
  int shadowsEnabled;
  int showMode;
  float _pad0;
} u_Frame;

struct ObjectData {
    mat4 model;
    mat4 normalMatrix;
    vec4 boundingSphere;
    vec4 boundsMinimum;
    vec4 boundsMaximum;
    uvec4 drawData;
};

layout(std430, set = 4, binding = 0) readonly buffer ObjectBuffer {
    ObjectData objects[];
};

struct DrawData {
    uint objectIndex;
    uint materialIndex;
    uint flags;
    uint padding;
};

layout(std430, set = 4, binding = 1) readonly buffer DrawDataBuffer {
    DrawData draws[];
};

void main() {
    vDrawIndex = gl_BaseInstanceARB;
    ObjectData objectData = objects[draws[vDrawIndex].objectIndex];
    vec4 worldPos = objectData.model * vec4(inPosition, 1.0);

    mat3 normalMatrix = mat3(objectData.normalMatrix);

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
