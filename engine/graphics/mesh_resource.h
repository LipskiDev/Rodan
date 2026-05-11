#pragma once

#include "glm/ext/matrix_float4x4.hpp"
#include "glm/ext/vector_float4.hpp"
#include "rhi/rhi_handles.h"
#include "rhi/rhi_pipeline.h"
#include "scene/bounding_box.h"
#include <cstdint>
#include <vector>
namespace Rodan {
struct Submesh {
  uint32_t firstIndex;
  uint32_t indexCount;
  uint32_t materialSlot;
  bool hasTangents = false;
};

struct MeshVertex {
  glm::vec3 position;
  glm::vec3 normal;
  glm::vec2 uv;
  glm::vec4 tangent;
};

std::vector<Velos::RHI::VertexBufferLayoutDesc> GetMeshVertexLayout();

class MeshResource {
public:
  Velos::RHI::BufferHandle vertexBuffer;
  Velos::RHI::BufferHandle indexBuffer;

  uint32_t vertexCount = 0;
  uint32_t indexCount = 0;

  std::vector<Submesh> submeshes;

  AABB aabb;
};

} // namespace Rodan
