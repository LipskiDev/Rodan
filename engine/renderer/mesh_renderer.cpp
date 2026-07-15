#include "renderer/mesh_renderer.h"
#include "graphics/mesh_resource.h"
#include "graphics/shaders_types.h"

#include <stdexcept>

namespace Rodan {
void MeshRenderer::Draw(ICommandList *cmd, const MeshResource &mesh,
                        const std::vector<const MaterialResource *> &materials,
                        PipelineHandle pipeline) {
  if (!cmd) {
    throw std::runtime_error("MeshRenderer::Draw: cmd is null");
  }

  cmd->BindVertexBuffer(0, mesh.vertexBuffer, 0);
  cmd->BindIndexBuffer(mesh.indexBuffer, IndexType::U32, 0);

  for (const Submesh &submesh : mesh.submeshes) {
    const MaterialResource *mat = nullptr;

    if (submesh.materialSlot < materials.size()) {
      mat = materials[submesh.materialSlot];
    }

    DrawSubmesh(cmd, mesh, submesh, mat, pipeline);
  }
}

void MeshRenderer::DrawSubmesh(ICommandList *cmd, const MeshResource &mesh,
                               const Submesh &submesh,
                               const MaterialResource *material,
                               PipelineHandle pipeline) {
  if (!cmd)
    return;

  cmd->BindPipeline(pipeline);

  if (material) {
    cmd->SetBindings(pipeline, 0, GetTextureRegistry().GetBindingSet());
  }

  cmd->BindVertexBuffer(0, mesh.vertexBuffer, 0);
  cmd->BindIndexBuffer(mesh.indexBuffer, IndexType::U32, 0);

  cmd->DrawIndexed(submesh.indexCount, submesh.firstIndex, 0);
}

void MeshRenderer::DrawSubmeshBound(ICommandList *cmd, const Submesh &submesh) {
  cmd->DrawIndexed(submesh.indexCount, submesh.firstIndex, 0);
}

void MeshRenderer::DrawDepthOnly(ICommandList *cmd, const MeshResource &mesh,
                                 const Submesh &submesh) {
  if (!cmd) {
    return;
  }

  cmd->BindVertexBuffer(0, mesh.vertexBuffer, 0);
  cmd->BindIndexBuffer(mesh.indexBuffer, IndexType::U32, 0);

  cmd->DrawIndexed(submesh.indexCount, submesh.firstIndex, 0);
}
} // namespace Rodan
