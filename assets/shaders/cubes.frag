#version 460 core

layout(set = 0, binding = 0) uniform sampler2D uTexture;

layout(location = 0) in vec3 color;
layout(location = 1) in vec2 uv;

layout(location = 0) out vec4 out_FragColor;

void main() {
    out_FragColor = vec4(color, 1.0);
}
