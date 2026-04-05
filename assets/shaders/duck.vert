#version 460 core

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec2 inUV;

layout(location = 0) out vec2 fragUV;

layout(push_constant) uniform PushConstants {
    mat4 model;
    mat4 view;
    mat4 proj;
} pc;

void main() {
    fragUV = inUV;
    gl_Position = pc.proj * pc.view * pc.model * vec4(inPosition, 1.0);
}
