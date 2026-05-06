#pragma once
#include <core/types.h>
#include <graphics/material_resource.h>
#include <graphics/mesh_resource.h>
#include <scene/handles.h>

namespace Rodan {

struct RenderObjectDesc {
  MeshHandle mesh;
  std::vector<MaterialHandle> materials;
  glm::mat4 world = glm::mat4(1.0f);
  bool visible = true;
  uint32_t objectId = 0;
};

struct RenderObject {
  MeshHandle mesh;
  std::vector<MaterialHandle> materials;

  glm::mat4 world = glm::mat4(1.0f);

  bool visible = true;
  uint32_t objectId = 0;
};
} // namespace Rodan
