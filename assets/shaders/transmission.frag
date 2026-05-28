#version 450

layout(location = 0) in vec3 vWorldNormal;
layout(location = 1) in vec3 vWorldPos;
layout(location = 2) in vec2 vUV;
layout(location = 3) in vec3 vTangent;
layout(location = 4) in vec3 vBitangent;
layout(location = 5) in vec3 vNormal;

layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform sampler2D u_BaseColor;
layout(set = 0, binding = 1) uniform sampler2D u_Normal;
layout(set = 0, binding = 2) uniform sampler2D u_MetallicRoughness;

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

struct MaterialData {
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
};

layout(std430, set = 1, binding = 2) readonly buffer MaterialBuffer {
    MaterialData materials[];
};

layout(push_constant) uniform Push {
    mat4 model;
    int showMode;
    int hasTangents;
    int materialIndex;
    int _pad0;
} pc;

#define material materials[pc.materialIndex]

layout(set = 2, binding = 1) uniform samplerCube u_PrefilterMap;
layout(set = 3, binding = 0) uniform sampler2D u_OpaqueScene;

const float MAX_REFLECTION_LOD = 5.0;

float saturate(float x)
{
    return clamp(x, 0.0, 1.0);
}

float dielectricF0(float ior)
{
    float f = (ior - 1.0) / (ior + 1.0);
    return f * f;
}

vec3 getNormal()
{
    vec3 N = normalize(vWorldNormal);

    if (length(vTangent) > 0.001 &&
        length(vBitangent) > 0.001 &&
        length(vNormal) > 0.001)
    {
        vec3 normalSample = texture(u_Normal, vUV).xyz * 2.0 - 1.0;

        mat3 TBN = mat3(
            normalize(vTangent),
            normalize(vBitangent),
            normalize(vNormal)
        );

        N = normalize(TBN * normalSample);
    }

    return N;
}

vec3 applyVolumeAttenuation(
    vec3 color,
    float distance,
    vec3 attenuationColor,
    float attenuationDistance)
{
    if (attenuationDistance <= 0.0 || distance <= 0.0) {
        return color;
    }

    vec3 safeColor = max(attenuationColor, vec3(0.001));
    vec3 attenuationCoefficient = -log(safeColor) / attenuationDistance;
    vec3 transmittance = exp(-attenuationCoefficient * distance);

    return color * transmittance;
}

vec3 sampleOpaqueScene(vec2 uv, float roughness)
{
    uv = clamp(uv, vec2(0.001), vec2(0.999));

    float lod = roughness * roughness * 4.0;

    return textureLod(u_OpaqueScene, uv, lod).rgb;
}

vec2 projectWorldToScreenUV(vec3 worldPos)
{
    vec4 clip = frame.proj * frame.view * vec4(worldPos, 1.0);

    if (clip.w <= 0.0) {
        return gl_FragCoord.xy / max(frame.viewportSize, vec2(1.0));
    }

    vec2 uv = clip.xy / clip.w;
    uv = uv * 0.5 + 0.5;

    return uv;
}

void main()
{
    vec3 N = getNormal();

    vec3 camPos = vec3(inverse(frame.view)[3]);
    vec3 V = normalize(camPos - vWorldPos);

    vec4 baseSample = texture(u_BaseColor, vUV);
    vec3 baseColor = material.baseColorFactor.rgb * baseSample.rgb;
    float alpha = material.baseColorFactor.a * baseSample.a;

    vec4 mrSample = texture(u_MetallicRoughness, vUV);

    float perceptualRoughness =
        clamp(material.roughnessFactor * mrSample.g, 0.04, 1.0);

    float ior =
        material.ior > 0.0 ? material.ior : 1.5;

    float transmission =
        saturate(material.transmissionFactor);

    float transmissionRoughness =
        perceptualRoughness *
        clamp(ior * 2.0 - 2.0, 0.0, 1.0);

    vec2 screenUV =
        gl_FragCoord.xy / max(frame.viewportSize, vec2(1.0));

    vec2 transmissionUV = screenUV;

    float thickness = max(material.thicknessFactor, 0.0);
    float safeThickness = min(thickness, 0.05);

    if (safeThickness > 0.0001) {
        vec3 refracted = refract(-V, N, 1.0 / ior);
        vec3 exitPos = vWorldPos + normalize(refracted) * safeThickness;
        transmissionUV = projectWorldToScreenUV(exitPos);
    }

    vec3 transmitted =
        sampleOpaqueScene(transmissionUV, transmissionRoughness);

    transmitted =
        applyVolumeAttenuation(
            transmitted,
            safeThickness,
            material.attenuationColorDistance.rgb,
            material.attenuationColorDistance.a
        );

    if (material.thicknessFactor > 0.0) {
        transmitted *= baseColor;
    }

    float NoV = saturate(dot(N, V));

    float f0 = dielectricF0(ior);

    float fresnel =
        f0 +
        (1.0 - f0) *
        pow(1.0 - NoV, 5.0);

    vec3 R = normalize(reflect(-V, N));

    vec3 reflected =
        textureLod(
            u_PrefilterMap,
            R,
            perceptualRoughness *
            perceptualRoughness *
            MAX_REFLECTION_LOD
        ).rgb;

    vec3 glass =
        transmitted * (1.0 - fresnel) +
        reflected * fresnel;

    vec3 finalColor =
        mix(baseColor, glass, transmission);

    outColor = vec4(finalColor, alpha);
}
