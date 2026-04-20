#version 450

layout(location = 0) in vec3 vWorldNormal;
layout(location = 1) in vec3 vWorldPos;
layout(location = 2) in vec2 vUV;
layout(location = 3) in mat3 vTBN;

layout(set = 0, binding = 0) uniform sampler2D u_BaseColor;
layout(set = 0, binding = 1) uniform sampler2D u_Normal;
layout(set = 0, binding = 2) uniform sampler2D u_MetallicRoughness;

layout(location = 0) out vec4 outColor;

layout(push_constant) uniform PushConstants {
    mat4 model;
    mat4 view;
    mat4 proj;

    int showMode;

    vec4 baseColorFactor;
    float metallicFactor;
    float roughnessFactor;
    int hasMaterial;
} pc;

const float PI = 3.14159265359;

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

vec3 FresnelSchlick(float cosTheta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

void main() {
    vec4 baseTex = texture(u_BaseColor, vUV);
    vec4 mrTex   = texture(u_MetallicRoughness, vUV);
    vec3 nTex = texture(u_Normal, vUV).xyz * 2.0 - 1.0;
    vec3 N = normalize(vTBN * nTex);

    vec3 baseColor = pc.baseColorFactor.rgb * baseTex.rgb;
    float alpha = pc.baseColorFactor.a * baseTex.a;

    float metallic = pc.metallicFactor * mrTex.b;
    float roughness = pc.roughnessFactor * mrTex.g;

    if (pc.hasMaterial == 0) {
        metallic = 0.0;
        roughness = 0.5;
    }

    metallic = clamp(metallic, 0.0, 1.0);
    roughness = clamp(roughness, 0.045, 1.0);

    vec3 camPos = vec3(inverse(pc.view)[3]);
    vec3 V = normalize(camPos - vWorldPos);

    vec3 L = normalize(vec3(0.4, 1.0, 0.3));
    vec3 H = normalize(V + L);

    vec3 radiance = vec3(4.0);

    float NdotL = max(dot(N, L), 0.0);
    float NdotV = max(dot(N, V), 0.0);
    float HdotV = max(dot(H, V), 0.0);

    vec3 F0 = mix(vec3(0.04), baseColor, metallic);

    float NDF = DistributionGGX(N, H, roughness);
    float G   = GeometrySmith(N, V, L, roughness);
    vec3  F   = FresnelSchlick(HdotV, F0);

    vec3 numerator = NDF * G * F;
    float denominator = max(4.0 * NdotV * NdotL, 0.0001);
    vec3 specular = numerator / denominator;

    vec3 kS = F;
    vec3 kD = (vec3(1.0) - kS) * (1.0 - metallic);

    vec3 diffuse = kD * baseColor / PI;
    vec3 direct = (diffuse + specular) * radiance * NdotL;
    vec3 ambient = vec3(0.03) * baseColor;

    vec3 color = ambient + direct;
    color = color / (color + vec3(1.0));
    color = pow(color, vec3(1.0 / 2.2));

    if (alpha < 0.5)
        discard;

    if (pc.showMode == 0) {
        outColor = vec4(baseTex.rgb, 1.0);
        return;
    }

    if (pc.showMode == 1) {
        outColor = vec4(nTex * 0.5 + 0.5, 1.0);
        return;
    }

    if (pc.showMode == 2) {
        outColor = vec4(mrTex.rgb, 1.0);
        return;
    }

    if (pc.showMode == 3) {
        vec3 T = normalize(vTBN[0]); 
        outColor = vec4(T * 0.5 + 0.5, 1.0);
        return;
    }

    outColor = vec4(color, alpha);
}
