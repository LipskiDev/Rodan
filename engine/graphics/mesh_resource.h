#pragma once

#include "glm/ext/matrix_float4x4.hpp"
#include "glm/ext/vector_float4.hpp"
#include "rhi/rhi_handles.h"
#include <cstdint>
#include <vector>
namespace Rodan {
struct Submesh {
  uint32_t firstIndex;
  uint32_t indexCount;
  uint32_t materialSlot;
};

struct MVPPushConstants {
  glm::mat4 model;
  glm::mat4 view;
  glm::mat4 proj;
};

struct MaterialPushConstants {
  glm::vec4 baseColorFactor;
  float metallicFactor;
  float roughnessFactor;
  int hasMaterial;
};

using namespace Velos::RHI;
class MeshResource {
public:
  BufferHandle vertexBuffer;
  BufferHandle indexBuffer;

  std::vector<Submesh> submeshes;
};

} // namespace Rodan
