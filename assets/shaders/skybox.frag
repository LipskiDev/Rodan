#version 450

layout(location = 0) in vec3 vDirection;
layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform samplerCube u_Environment;

void main() {
    vec3 color = texture(u_Environment, normalize(vDirection)).rgb;

    // Temporary tonemap/gamma for HDR skybox display.
    color = color / (color + vec3(1.0));
    color = pow(color, vec3(1.0 / 2.2));

    outColor = vec4(color, 1.0);
}
