#version 450

layout(location = 0) in vec3 inPos;

layout(location = 0) out vec3 vDir;

layout(push_constant) uniform Push {
    mat4 view;
    mat4 proj;
} pc;

void main()
{
    mat4 view = mat4(mat3(pc.view));

    vec4 pos = pc.proj * view * vec4(inPos, 1.0);
    
    gl_Position = pos.xyww;

    vDir = inPos;
}
