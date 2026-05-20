#version 450

layout(location = 0) in vec3 aPosition;

layout(location = 0) out vec3 vLocalPos;

layout(push_constant) uniform PushConstants {
    mat4 view;
    mat4 proj;
    float roughness;
} pc;

void main()
{
    vLocalPos = aPosition;

    gl_Position =
        pc.proj *
        pc.view *
        vec4(aPosition, 1.0);
}
