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

layout(push_constant) uniform Push {
    mat4 model;
    int showMode;
    int hasTangents;
} pc;

layout(set = 1, binding = 1) uniform FrameData {
    mat4 view;
    mat4 proj;
    mat4 lightViewProj;

    vec4 lightDirection;
    vec4 lightColor;

    vec2 viewportSize;

    float lightIntensity;
    int shadowsEnabled;
    int showMode;
    float _pad0;
} frame;

void main()
{
    vec4 worldPos = pc.model * vec4(inPosition, 1.0);

    mat3 normalMatrix = mat3(transpose(inverse(pc.model)));

    vWorldPos = worldPos.xyz;
    vWorldNormal = normalize(normalMatrix * inNormal);
    vNormal = vWorldNormal;
    vUV = inUV;

    if (pc.hasTangents == 1) {
        vTangent = normalize(normalMatrix * inTangent.xyz);
        vBitangent = normalize(cross(vWorldNormal, vTangent) * inTangent.w);
    } else {
        vTangent = vec3(0.0);
        vBitangent = vec3(0.0);
    }

    gl_Position = frame.proj * frame.view * worldPos;
}
