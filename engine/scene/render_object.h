#pragma once
#include "scene/transform.h"
#include <core/types.h>
#include <graphics/material_resource.h>
#include <graphics/mesh_resource.h>
#include <scene/handles.h>

namespace Rodan {

struct RenderObjectDesc {
  MeshHandle mesh;
  std::vector<MaterialHandle> materials;

  Transform transform;

  bool visible = true;
  uint32_t objectId = 1;
};

struct RenderObject {
  MeshHandle mesh;
  std::vector<MaterialHandle> materials;

  Transform transform;

  bool visible = true;
  uint32_t objectId = 1;
};
} // namespace Rodan
