#include <cassert>
#include <scene/render_world.h>

namespace Rodan {

MeshHandle RenderWorld::AddMesh(MeshResource mesh) {
  const uint32_t id = nextMeshIndex_++;
  meshes_.emplace(id, std::move(mesh));
  return MeshHandle{id};
}

MaterialHandle RenderWorld::AddMaterial(MaterialResource material) {
  const uint32_t id = nextMaterialIndex_++;
  materials_.emplace(id, std::move(material));
  return MaterialHandle{id};
}

DirectionalLightHandle
RenderWorld::AddDirectionalLight(const DirectionalLight &light) {
  directionalLights_.push_back({.direction = light.direction,
                                .color = light.color,
                                .intensity = light.intensity,
                                .castsShadow = light.castsShadow});

  return static_cast<uint32_t>(directionalLights_.size() - 1);
}

void RenderWorld::Clear() {
  objects_.clear();
  directionalLights_.clear();
  meshes_.clear();
  materials_.clear();

  nextMeshIndex_ = 1;
  nextMaterialIndex_ = 1;
}

RenderObjectHandle RenderWorld::CreateObject(const RenderObjectDesc &desc) {
  assert(desc.mesh.IsValid());

  const uint32_t id = static_cast<uint32_t>(objects_.size());

  RenderObject object{};
  object.mesh = desc.mesh;
  object.materials = desc.materials;
  object.transform = desc.transform;
  object.visible = desc.visible;
  object.objectId = desc.objectId;

  objects_.push_back(std::move(object));

  return RenderObjectHandle{id};
}

void RenderWorld::SetTransform(RenderObjectHandle handle,
                               const Transform transform) {
  assert(handle.id < objects_.size());

  objects_[handle.id].transform = transform;
}

void RenderWorld::SetVisible(RenderObjectHandle handle, bool visible) {
  assert(handle.id < objects_.size());

  objects_[handle.id].visible = visible;
}

const std::vector<RenderObject> &RenderWorld::GetObjects() const {
  return objects_;
}

const MeshResource &RenderWorld::GetMesh(MeshHandle handle) const {
  return meshes_.at(handle.id);
}

const MaterialResource &RenderWorld::GetMaterial(MaterialHandle handle) const {
  assert(handle.IsValid());
  return materials_.at(handle.id);
}

const std::vector<DirectionalLight> &RenderWorld::GetDirectionalLights() const {
  return directionalLights_;
}

DirectionalLight &
RenderWorld::GetDirectionalLight(DirectionalLightHandle handle) {
  return directionalLights_[handle];
}
} // namespace Rodan
