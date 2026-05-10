#pragma once

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
    lower = glm::min(lower, p);
    upper = glm::max(upper, p);
  }
};
} // namespace Rodan
