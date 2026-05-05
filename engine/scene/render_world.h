#pragma once

#include "graphics/material_resource.h"
#include "scene/render_object.h"
#include <vector>

namespace Rodan {
class RenderWorld {
public:
  MeshHandle AddMesh(MeshResource mesh);
  MaterialHandle AddMaterial(MaterialResource material);

  RenderObjectHandle CreateObject(const RenderObjectDesc &desc);

  void SetTransform(RenderObjectHandle handle, const glm::mat4 &world);
  void SetVisible(RenderObjectHandle handle, bool visible);

  const std::vector<RenderObject> &GetObjects() const;
  const MeshResource &GetMesh(MeshHandle handle) const;
  const MaterialResource &GetMaterial(MaterialHandle handle) const;

private:
  uint32_t nextMeshIndex_ = 1;
  std::unordered_map<uint32_t, MeshResource> meshes_;

  uint32_t nextMaterialIndex_ = 1;
  std::unordered_map<uint32_t, MaterialResource> materials_;

  std::vector<RenderObject> objects_;
};
} // namespace Rodan
