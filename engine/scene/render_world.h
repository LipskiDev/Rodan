#pragma once

#include "graphics/environment/environment_map.h"
#include "graphics/material_resource.h"
#include "scene/handles.h"
#include "scene/light.h"
#include "scene/render_object.h"
#include <iterator>
#include <memory>
#include <vector>

namespace Rodan {
class RenderWorld {
public:
  MeshHandle AddMesh(MeshResource mesh);
  MaterialHandle AddMaterial(MaterialResource material);
  DirectionalLightHandle AddDirectionalLight(const DirectionalLight &light);

  RenderObjectHandle CreateObject(const RenderObjectDesc &desc);

  void SetTransform(RenderObjectHandle handle, const Transform transform);
  void SetVisible(RenderObjectHandle handle, bool visible);
  void SetEnvironment(std::shared_ptr<EnvironmentMap> env);

  void SetShadows(bool renderShadows) { renderShadows_ = renderShadows; }

  const std::vector<RenderObject> &GetObjects() const;
  const MeshResource &GetMesh(MeshHandle handle) const;
  const MaterialResource &GetMaterial(MaterialHandle handle) const;
  const std::vector<DirectionalLight> &GetDirectionalLights() const;
  const std::unordered_map<uint32_t, MaterialResource> &GetMaterials() const;
  DirectionalLight &GetDirectionalLight(DirectionalLightHandle handle);

  void Clear();

private:
  void RenderDirectionalShadowMap(DirectionalLight light);

private:
  uint32_t nextMeshIndex_ = 1;
  std::unordered_map<uint32_t, MeshResource> meshes_;

  uint32_t nextMaterialIndex_ = 1;
  std::unordered_map<uint32_t, MaterialResource> materials_;

  std::vector<DirectionalLight> directionalLights_;

  std::vector<RenderObject> objects_;

  bool renderShadows_ = true;

  std::shared_ptr<EnvironmentMap> environment_;
};
} // namespace Rodan
