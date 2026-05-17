#version 450

layout(location = 0) in vec3 aPosition;

layout(location = 0) out vec3 vDirection;

layout(push_constant) uniform PushConstants {
    mat4 view;
    mat4 proj;
} pc;

void main() {
    vDirection = aPosition;

    vec4 clip = pc.proj * pc.view * vec4(aPosition, 1.0);

    // Force skybox to far plane.
    gl_Position = clip.xyww;
}
