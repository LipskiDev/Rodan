#pragma once
#include <glm/glm.hpp>

namespace Rodan {

struct StaticMeshPushConstants {
  glm::mat4 model; // 64

  glm::vec4 baseColorFactor; // 16

  float metallicFactor;  // 4
  float roughnessFactor; // 4
  float alphaCutoff;     // 4

  int showMode;    // 4
  int hasMaterial; // 4
  int alphaMode;   // 4
  int hasTangents; // 4
};

struct ShadowPushConstants {
  glm::mat4 model;
  glm::mat4 lightViewProj;
};

} // namespace Rodan
