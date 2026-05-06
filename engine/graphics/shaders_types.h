#pragma once
#include <glm/glm.hpp>

namespace Rodan {
struct MVPPushConstants {
  glm::mat4 model;
  glm::mat4 view;
  glm::mat4 proj;
  int showMode;
  int pad[3];
};

struct ShadowPushConstants {
  glm::mat4 model;
  glm::mat4 lightViewProj;
};

struct MaterialPushConstants {
  glm::vec4 baseColorFactor;

  float metallicFactor;
  float roughnessFactor;
  int hasMaterial;
  int alphaMode;

  float alphaCutoff;
  float pad[3];
};
} // namespace Rodan
