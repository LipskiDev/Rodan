#version 450

layout(location = 0) in vec3 vWorldNormal;
layout(location = 1) in vec3 vWorldPos;
layout(location = 2) in vec2 vUV;

layout(location = 0) out vec4 outColor;

void main() {
    vec3 N = normalize(vWorldNormal);
    vec3 L = normalize(vec3(0.4, 1.0, 0.3));

    float ndotl = max(dot(N, L), 0.0);
    vec3 baseColor = vec3(0.75);
    vec3 color = baseColor * (0.15 + 0.85 * ndotl);

    outColor = vec4(color, 1.0);
}
