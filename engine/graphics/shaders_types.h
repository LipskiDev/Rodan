#pragma once
#include <glm/glm.hpp>

namespace Rodan {

struct StaticMeshPushConstants {
  uint32_t gpuObjectIndex;
  int showMode;
  int hasTangents;
  int materialIndex;
};

struct ShadowPushConstants {
  glm::mat4 model;
  glm::mat4 lightViewProj;
};

} // namespace Rodan
