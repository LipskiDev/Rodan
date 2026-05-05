#pragma once
#include <core/types.h>
#include <graphics/material_resource.h>
#include <graphics/mesh_resource.h>

namespace Rodan {
struct MeshTag {};
struct MaterialTag {};
struct RenderObjectTag {};

using MeshHandle = Handle<MeshTag>;
using MaterialHandle = Handle<MaterialTag>;
using RenderObjectHandle = Handle<RenderObjectTag>;

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
