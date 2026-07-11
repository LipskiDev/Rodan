#pragma once

#include "glm/fwd.hpp"
#include "rhi/command_list.h"
#include "rhi/handles.h"
#include <glm/glm.hpp>

#include <rhi/device.h>

namespace Rodan::Debug {

using namespace glm;

class LineRenderer3D {
public:
  LineRenderer3D(Velos::RHI::IDevice *device);
  ~LineRenderer3D();

  void clear();
  void line(const vec3 &p1, const vec3 &p2, const vec4 &color);
  void plane(const vec3 &orig, const vec3 &v1, const vec3 &v2, int n1, int n2,
             float s1, float s2, const vec4 &color, const vec4 &outlineColor);
  void box(const mat4 &m, const vec3 &size, const vec4 &color);
  void aabb(vec3 lower, vec3 upper, const vec4 &color);
  void frustum(const mat4 &camView, const mat4 &camProj, const vec4 &color);
  void arrow(const vec3 &start, const vec3 &end, const vec4 &color,
             float headSize = 0.1f);

  void render(Velos::RHI::ICommandList &cmd, const mat4 &viewProj);

private:
  Velos::RHI::IDevice *device_;

  struct LineData {
    vec4 pos;
    vec4 color;
  };
  std::vector<LineData> lines_;

  Velos::RHI::BufferHandle vertexBuffer_ = {};
  Velos::RHI::PipelineHandle pipeline_;

  Velos::RHI::ShaderHandle vertexShader_;
  Velos::RHI::ShaderHandle fragmentShader_;

  Velos::u64 vertexBufferSize_ = 0;
};

class LineRenderer2D {
public:
  void clear() { lines_.clear(); }
  void line(const vec2 &p1, const vec2 &p2, const vec4 &c) {
    lines_.push_back({.p1 = p1, .p2 = p2, .color = c});
  }
  void render(const char *nameImGuiWindow);

private:
  struct LineData {
    vec2 p1, p2;
    vec4 color;
  };
  std::vector<LineData> lines_;
};
}; // namespace Rodan::Debug
