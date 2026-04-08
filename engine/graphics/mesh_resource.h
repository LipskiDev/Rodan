#pragma once

#include "rhi/rhi_handles.h"
#include <cstdint>
#include <vector>
namespace Rodan {
struct Submesh {
  uint32_t firstIndex;
  uint32_t indexCount;
  uint32_t materialSlot;
};

using namespace Velos::RHI;
class MeshResource {
public:
  BufferHandle vertexBuffer;
  BufferHandle indexBuffer;

  std::vector<Submesh> submeshes;
};

} // namespace Rodan
