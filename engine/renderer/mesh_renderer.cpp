#include "renderer/mesh_renderer.h"

#include <stdexcept>

namespace Rodan {

void MeshRenderer::Draw(ICommandList *cmd, const StaticMeshInstance &instance) {
  if (!cmd) {
    throw std::runtime_error("MeshRenderer::Draw: cmd is null");
  }

  if (!instance.mesh) {
    return;
  }

  const MeshResource &mesh = *instance.mesh;

  cmd->BindVertexBuffer(0, mesh.vertexBuffer, 0);
  cmd->BindIndexBuffer(mesh.indexBuffer, IndexType::U32, 0);

  for (const Submesh &submesh : mesh.submeshes) {
    cmd->DrawIndexed(submesh.indexCount, submesh.firstIndex, 0);
  }
}

} // namespace Rodan
