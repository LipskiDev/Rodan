#pragma once

#include "glm/ext/matrix_float4x4.hpp"
#include "graphics/mesh_resource.h"
#include <memory>
namespace Rodan {
struct StaticMeshInstance {
  std::shared_ptr<MeshResource> mesh;
  glm::mat4 worldTransform{1.0f};
  glm::mat4 localTransform{1.0};
};
} // namespace Rodan
