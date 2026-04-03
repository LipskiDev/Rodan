#version 460

#extension GL_EXT_buffer_reference : require
#extension GL_EXT_scalar_block_layout : require
#extension GL_ARB_gpu_shader_int64 : require

layout (location = 0) in vec4 in_color;
layout (location = 0) out vec4 out_color;

void main() {
  out_color = in_color;
}
