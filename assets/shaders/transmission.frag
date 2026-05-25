#version 450

layout(location = 0) in vec3 vWorldPos;
layout(location = 1) in vec3 vNormal;
layout(location = 2) in vec2 vUV;

layout(location = 0) out vec4 outColor;

layout(set = 1, binding = 1) uniform FrameData {
    mat4 lightViewProj;
    vec3 lightDirection;
    float lightIntensity;

    vec3 lightColor;
    int renderShadows;

    mat4 proj;
    mat4 view;

    int showMode;
    vec2 viewportSize;
} frame;

layout(set = 1, binding = 2) uniform MaterialData {
    vec4 baseColorFactor;

    float metallicFactor;
    float roughnessFactor;
    float alphaCutoff;
    int alphaMode;

    int hasMaterial;
    float transmissionFactor;
    float thicknessFactor;
    float ior;

    vec4 attenuationColorDistance;
} material;

layout(set = 3, binding = 0) uniform sampler2D u_OpaqueScene;

void main()
{
    ivec2 size = textureSize(u_OpaqueScene, 0);
    ivec2 pixel = ivec2(gl_FragCoord.xy);
    pixel = clamp(pixel, ivec2(0), size - ivec2(1));

    vec3 opaqueScene = texelFetch(u_OpaqueScene, pixel, 0).rgb;

    float thickness = max(material.thicknessFactor, 0.0);
    float attenuationDistance = material.attenuationColorDistance.a;

    float transmission = 0.85;
    vec3 attenuation = vec3(0.35, 0.8, 1.0); // blue glass
    vec3 tint = vec3(0.35, 0.8, 1.0);

    vec3 transmitted = opaqueScene * attenuation;
    vec3 finalColor = mix(tint, transmitted, transmission);

    outColor = vec4(finalColor, 1.0);
}
