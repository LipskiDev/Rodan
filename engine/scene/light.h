#pragma once

#include "glm/ext/vector_float3.hpp"
namespace Rodan {
struct DirectionalLight {
  glm::vec3 direction = glm::vec3(0.0f, -1.0f, 0.0f);
  glm::vec3 color = glm::vec3(1.0f);
  float intensity = 1.0f;
  bool castsShadow = false;
};

struct PointLight {
  glm::vec3 position = glm::vec3(0.0f);
  glm::vec3 color = glm::vec3(1.0f);
  float intensity = 1.0f;
  float range = 10.0f;
  bool castsShadow = false;
};

struct SpotLight {
  glm::vec3 position = glm::vec3(0.0f);
  glm::vec3 direction = glm::vec3(0.0f, -1.0f, 0.0f);
  glm::vec3 color = glm::vec3(1.0f);
  float intensity = 1.0f;
  float range = 10.0f;
  float innerConeAngle = 0.5f;
  float outerConeAngle = 0.75f;
  bool castsShadow = false;
};
} // namespace Rodan
