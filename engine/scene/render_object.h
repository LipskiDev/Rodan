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
  uint32_t objectId = 0;
};

struct RenderObject {
  MeshHandle mesh;
  std::vector<MaterialHandle> materials;

  Transform localTransform;
  Transform worldTransform;

  bool visible = true;
  uint32_t objectId = 0;
};

struct GPUSceneObject {
	glm::mat4 model;
	glm::mat4 normalMatrix;

	glm::vec4 boundingSphere; // xyz = position, w = radius
	glm::vec4 boundsMinimum; // AABB minimum
	glm::vec4 boundsMaximum; // AABB maximum

	glm::uvec4 drawData;
	// x: batch index
	// y: material index
	// z: object ID
	// w: flags
};

struct GPUDrawData {
  uint32_t objectIndex;
  uint32_t materialIndex;
  uint32_t flags;
  uint32_t padding;
};

constexpr uint32_t GPUDrawFlag_HasTangents = 1u << 0;
} // namespace Rodan
