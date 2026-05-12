#pragma once

#include "glm/ext/matrix_float4x4.hpp"
#include "glm/ext/vector_float3.hpp"
#include <glm/common.hpp>
#include <unistd.h>

namespace Rodan {
struct AABB {
  glm::vec3 lower = glm::vec3{0.0f};
  glm::vec3 upper = glm::vec3{0.0f};

  glm::vec3 Center() const { return 0.5f * (upper + lower); }

  glm::vec3 Extents() const { return upper - lower; }

  float MaxExtent() const {
    glm::vec3 e = Extents();
    return std::max(e.x, std::max(e.y, e.z));
  }

  void Expand(glm::vec3 p) {
    if (first) {
      lower = p;
      upper = p;
      first = false;
      return;
    }
    lower = glm::min(lower, p);
    upper = glm::max(upper, p);
  }

  AABB Transform(const glm::mat4 &m) const {
    AABB out;

    glm::vec3 corners[8] = {
        {lower.x, lower.y, lower.z}, {upper.x, lower.y, lower.z},
        {lower.x, upper.y, lower.z}, {upper.x, upper.y, lower.z},
        {lower.x, lower.y, upper.z}, {upper.x, lower.y, upper.z},
        {lower.x, upper.y, upper.z}, {upper.x, upper.y, upper.z},
    };

    for (const glm::vec3 &c : corners) {
      out.Expand(glm::vec3(m * glm::vec4(c, 1.0f)));
    }

    return out;
  }

private:
  bool first = true;
};
} // namespace Rodan
