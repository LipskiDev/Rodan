#version 450

layout(location = 0) in vec3 inPos;
layout(location = 1) in vec3 inColor;

layout(location = 0) out vec3 vColor;

layout(push_constant) uniform PushConstants {
    mat4 uMVP;
} pc;

void main() {
    vColor = inColor;
    gl_Position = pc.uMVP * vec4(inPos, 1.0);
}
