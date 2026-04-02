#version 450

layout(location = 0) in vec3 vDir;
layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform samplerCube u_Texture;

void main()
{
    vec3 dir = normalize(vDir);
    dir.y = -dir.y;
    outColor = texture(u_Texture, normalize(dir));
}
