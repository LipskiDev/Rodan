#version 460

#extension GL_EXT_buffer_reference : require
#extension GL_EXT_scalar_block_layout : require
#extension GL_ARB_gpu_shader_int64 : require

layout(location = 0) out vec4 out_color;

struct Vertex {
  vec4 pos;
  vec4 rgba;
};

layout(buffer_reference, std430) readonly buffer VertexBuffer {
  Vertex vertices[];
};

layout(push_constant) uniform PushConstants {
  mat4 mvp;
  VertexBuffer vb;
} pc;

void main() {
  Vertex v = pc.vb.vertices[gl_VertexIndex];
  out_color = v.rgba;
  gl_Position = pc.mvp * v.pos;
}
