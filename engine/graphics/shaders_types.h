#pragma once
#include <glm/glm.hpp>

namespace Rodan {

struct StaticMeshPushConstants {
  glm::mat4 model; // 64

  int showMode;
  int hasTangents;
  int materialIndex;
  int _pad0;
};

struct ShadowPushConstants {
  glm::mat4 model;
  glm::mat4 lightViewProj;
};

} // namespace Rodan
