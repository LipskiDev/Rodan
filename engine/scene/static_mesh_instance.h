#pragma once

#include "glm/ext/matrix_float4x4.hpp"
#include "graphics/mesh_resource.h"
#include "scene/transform.h"
#include <memory>
namespace Rodan {
struct StaticMeshInstance {
  std::shared_ptr<MeshResource> mesh;
  glm::mat4 worldMatrix{1.0f};
  Transform localTransform;
};
} // namespace Rodan
