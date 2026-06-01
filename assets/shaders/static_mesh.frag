#version 450

layout(location = 0) in vec3 vWorldNormal;
layout(location = 1) in vec3 vWorldPos;
layout(location = 2) in vec2 vUV;
layout(location = 3) in vec3 vTangent;
layout(location = 4) in vec3 vBitangent;
layout(location = 5) in vec3 vNormal;

layout(set = 0, binding = 0) uniform sampler2D u_BaseColor;
layout(set = 0, binding = 1) uniform sampler2D u_Normal;
layout(set = 0, binding = 2) uniform sampler2D u_MetallicRoughness;
layout(set = 0, binding = 3) uniform sampler2D u_OcclusionTexture;

layout(set = 1, binding = 0) uniform sampler2D u_ShadowMap;
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
} u_Frame;

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

layout(set = 2, binding = 0) uniform samplerCube u_IrradianceMap;
layout(set = 2, binding = 1) uniform samplerCube u_PrefilterMap;
layout(set = 2, binding = 2) uniform sampler2D u_BRDFLUT;

layout(set = 3, binding = 0) uniform sampler2D u_OpaqueScene;

layout(location = 0) out vec4 outColor;

layout(push_constant) uniform PushConstants {
    mat4 model;              // offset 0,   size 64

    int showMode;           
    int hasTangents;    
    int materialIndex;
    int _pad0;
} pc;

#define material materials[pc.materialIndex]

const float PI = 3.14159265359;

float ComputeShadow(vec3 worldPos, vec3 normal, vec3 lightDir) {
    vec4 lightSpace = u_Frame.lightViewProj * vec4(worldPos, 1.0);

    vec3 projCoords = lightSpace.xyz / lightSpace.w;
    projCoords.xy = projCoords.xy * 0.5 + 0.5;

    if (projCoords.x < 0.0 || projCoords.x > 1.0 ||
        projCoords.y < 0.0 || projCoords.y > 1.0 ||
        projCoords.z < 0.0 || projCoords.z > 1.0) {
        return 1.0;
    }

    float closestDepth = texture(u_ShadowMap, projCoords.xy).r;
    float currentDepth = projCoords.z;

    float NdotL = max(dot(normal, lightDir), 0.0);
    float bias = max(0.02 * (1.0 - NdotL), 0.002);

    return currentDepth - bias > closestDepth ? 0.0 : 1.0;
}

float DistributionGGX(vec3 N, vec3 H, float roughness) {
    float a = roughness * roughness;
    float a2 = a * a;

    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;

    float denom = NdotH2 * (a2 - 1.0) + 1.0;
    return a2 / max(PI * denom * denom, 0.000001);
}

float GeometrySchlickGGX(float NdotV, float roughness) {
    float r = roughness + 1.0;
    float k = (r * r) / 8.0;

    float denom = NdotV * (1.0 - k) + k;
    return NdotV / max(denom, 0.000001);
}

float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness) {
    float ggxV = GeometrySchlickGGX(max(dot(N, V), 0.0), roughness);
    float ggxL = GeometrySchlickGGX(max(dot(N, L), 0.0), roughness);
    return ggxV * ggxL;
}

vec3 FresnelSchlickRoughness(
    float cosTheta,
    vec3 F0,
    float roughness)
{
    return F0 + (max(vec3(1.0 - roughness), F0) - F0)
        * pow(1.0 - cosTheta, 5.0);
}

float saturate(float x)
{
    return clamp(x, 0.0, 1.0);
}

float dielectricF0(float ior)
{
    float f = (ior - 1.0) / (ior + 1.0);
    return f * f;
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

    float maxLod = float(textureQueryLevels(u_OpaqueScene) - 1);
    float lod = roughness * maxLod;

    return textureLod(u_OpaqueScene, uv, lod).rgb;
}

vec2 projectWorldToScreenUV(vec3 worldPos)
{
    vec4 clip = u_Frame.proj * u_Frame.view * vec4(worldPos, 1.0);

    if (clip.w <= 0.0) {
        return gl_FragCoord.xy / max(u_Frame.viewportSize, vec2(1.0));
    }

    vec2 uv = clip.xy / clip.w;
    uv = uv * 0.5 + 0.5;

    return uv;
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

void main() {
    vec4 baseTex = texture(u_BaseColor, vUV);
    vec4 mrTex   = texture(u_MetallicRoughness, vUV);
    vec3 nTex    = texture(u_Normal, vUV).xyz * 2.0 - 1.0;
    float ao     = texture(u_OcclusionTexture, vUV).r;

    vec3 N = getNormal();

    // SSBO material lookup
    vec3 baseColor = material.baseColorFactor.rgb * baseTex.rgb;
    float alpha    = material.baseColorFactor.a * baseTex.a;

    float metallic  = material.metallicFactor * mrTex.b;
    float roughness = material.roughnessFactor * mrTex.g;

    if (material.hasMaterial == 0) {
        metallic = 0.0;
        roughness = 0.5;
    }

    metallic = clamp(metallic, 0.0, 1.0);
    roughness = clamp(roughness, 0.08, 1.0);

    if (material.alphaMode == 1 && alpha < material.alphaCutoff) {
        discard;
    }

    vec3 camPos = vec3(inverse(u_Frame.view)[3]);
    vec3 V = normalize(camPos - vWorldPos);

    vec4 mrSample = texture(u_MetallicRoughness, vUV);
    float mrRoughness = mrSample.g;

    vec3 L = normalize(-u_Frame.lightDirection.xyz);
    vec3 H = normalize(V + L);

    vec3 radiance = u_Frame.lightColor.rgb * u_Frame.lightIntensity;

    float NdotL = max(dot(N, L), 0.0);
    float NdotV = max(dot(N, V), 0.0);
    float HdotV = max(dot(H, V), 0.0);

    vec3 F0 = mix(vec3(0.04), baseColor, metallic);

    vec3 irradiance = texture(u_IrradianceMap, N).rgb;

    vec3 R = reflect(-V, N);

    float maxReflectionLod =
        float(textureQueryLevels(u_PrefilterMap) - 1);

    vec3 prefilteredColor =
        textureLod(
            u_PrefilterMap,
            R,
            roughness * maxReflectionLod
        ).rgb;

    vec3 F_ibl =
        FresnelSchlickRoughness(NdotV, F0, roughness);

    vec3 kS_IBL = F_ibl;
    vec3 kD_IBL = (1.0 - kS_IBL) * (1.0 - metallic);

    vec3 diffuseIBL =
        irradiance * baseColor * kD_IBL * ao;

    vec2 brdf =
        texture(u_BRDFLUT, vec2(NdotV, roughness)).rg;

    vec3 specularIBL =
        prefilteredColor * (F_ibl * brdf.x + brdf.y) * ao;

    float NDF = DistributionGGX(N, H, roughness);
    float G   = GeometrySmith(N, V, L, roughness);
    vec3 F    = FresnelSchlickRoughness(HdotV, F0, roughness);

    vec3 specular =
        (NDF * G * F) /
        max(4.0 * NdotV * NdotL, 0.0001);

    vec3 kS = F;
    vec3 kD = (vec3(1.0) - kS) * (1.0 - metallic);

    vec3 diffuse = kD * baseColor / PI;

    float shadow = 1.0;

    if (u_Frame.shadowsEnabled == 1) {
        shadow = ComputeShadow(vWorldPos, N, L);
    }

    vec3 direct =
        (diffuse + specular) *
        radiance *
        NdotL *
        shadow;

    vec3 ambient =
        diffuseIBL + specularIBL;


    vec3 directDiffuse =
        diffuse * radiance * NdotL * shadow;

    vec3 directSpecular =
        specular * radiance * NdotL * shadow;

    vec3 f_diffuse = diffuseIBL + directDiffuse;
    vec3 f_specular = specularIBL + directSpecular;

    float transmission = saturate(material.transmissionFactor);

    vec3 color = mix(f_diffuse, f_specular, F_ibl);

    if(transmission > 0.001) {
      float ior = material.ior > 0.0 ? material.ior : 1.5;

      float perceptualRoughness = clamp(material.roughnessFactor * mrSample.g, 0.0, 1.0);
      float alphaRoughness = perceptualRoughness * perceptualRoughness;

      float transmissionRoughness = clamp(mrRoughness * mrRoughness, 0.0, 1.0);

      vec2 transmissionUV = gl_FragCoord.xy / max(u_Frame.viewportSize, vec2(1.0));

      float thickness = max(material.thicknessFactor, 0.0);
      float safeThickness = min(thickness, 0.05);

      if(safeThickness > 0.0001) {
        vec3 refracted = refract(-V, N, 1.0 / ior);
        vec3 exitPos = vWorldPos + normalize(refracted) * safeThickness;
        transmissionUV = projectWorldToScreenUV(exitPos);
      }

      vec3 transmitted = sampleOpaqueScene(transmissionUV, transmissionRoughness);

      float transmissionVisibility = transmission * (1.0 - perceptualRoughness * 0.65);

      transmitted = applyVolumeAttenuation(transmitted, safeThickness, material.attenuationColorDistance.rgb, material.attenuationColorDistance.a);

      if(material.attenuationColorDistance.a > 0.0) {
        transmitted *= baseColor;
      }

      float f0 = dielectricF0(ior);

      float fresnel = f0 + (1.0 - f0) * pow(1.0 - NdotV, 5.0);

      vec3 R = normalize(reflect(-V, N));

      vec3 reflected =
          textureLod(u_PrefilterMap, R, perceptualRoughness * perceptualRoughness * maxReflectionLod).rgb;

      vec3 glass = transmitted * (1.0 - fresnel) + reflected * fresnel;

      color = mix(baseColor, glass, transmissionVisibility);
    }

    outColor = vec4(color, alpha);
}
