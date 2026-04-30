#include "graphics/mesh_resource.h"
#include "assets/imported_scene.h"

namespace Rodan {

std::vector<Velos::RHI::VertexBufferLayoutDesc> GetMeshVertexLayout() {
  using namespace Velos::RHI;

  VertexBufferLayoutDesc layout{};
  layout.stride = sizeof(ImportedVertex);
  layout.inputRate = VertexInputRate::PerVertex;

  layout.attributes = {
      {
          .location = 0,
          .format = VertexFormat::Float32x3,
          .offset = offsetof(ImportedVertex, position),
      },
      {
          .location = 1,
          .format = VertexFormat::Float32x3,
          .offset = offsetof(ImportedVertex, normal),
      },
      {
          .location = 2,
          .format = VertexFormat::Float32x2,
          .offset = offsetof(ImportedVertex, uv),
      },
      {
          .location = 3,
          .format = VertexFormat::Float32x4,
          .offset = offsetof(ImportedVertex, tangent),
      },
  };

  return {layout};
}

} // namespace Rodan
