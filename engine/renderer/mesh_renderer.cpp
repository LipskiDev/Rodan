#include "renderer/mesh_renderer.h"
#include "graphics/mesh_resource.h"

#include <stdexcept>

namespace Rodan {

void MeshRenderer::Draw(ICommandList *cmd, const StaticMeshInstance &instance,
                        const std::vector<MaterialResource> &materials,
                        PipelineHandle pipeline) {
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
    if (submesh.materialSlot < materials.size()) {
      const MaterialResource &mat = materials[submesh.materialSlot];

      cmd->BindDescriptorSet(pipeline, 0, mat.descriptorSet);

      MaterialPushConstants matPc{};
      matPc.baseColorFactor = mat.baseColorFactor;
      matPc.metallicFactor = mat.metallicFactor;
      matPc.roughnessFactor = mat.roughnessFactor;
      matPc.hasMaterial = 1;

      cmd->PushConstants(ShaderStage::Vertex | ShaderStage::Fragment,
                         sizeof(MVPPushConstants),
                         sizeof(MaterialPushConstants), &matPc);
    }

    cmd->DrawIndexed(submesh.indexCount, submesh.firstIndex, 0);
  }
}

} // namespace Rodan
