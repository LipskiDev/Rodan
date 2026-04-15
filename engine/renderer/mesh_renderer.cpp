#include "renderer/mesh_renderer.h"
#include "graphics/mesh_resource.h"

#include <stdexcept>

namespace Rodan {

void MeshRenderer::Draw(ICommandList *cmd, const StaticMeshInstance &instance,
                        const ImportedScene &scene) {
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
    MaterialPushConstants matPc{};
    if (submesh.materialSlot >= 0 &&
        submesh.materialSlot < static_cast<int>(scene.materials.size())) {
      const ImportedMaterial &mat = scene.materials[submesh.materialSlot];
      matPc.baseColorFactor = mat.baseColorFactor;
      matPc.metallicFactor = mat.metallicFactor;
      matPc.roughnessFactor = mat.roughnessFactor;
      matPc.hasMaterial = 1;
    } else {
      matPc.baseColorFactor = glm::vec4(1.0f);
      matPc.metallicFactor = 1.0f;
      matPc.roughnessFactor = 1.0f;
      matPc.hasMaterial = 0;
    }
    cmd->PushConstants(ShaderStage::Vertex | ShaderStage::Fragment,
                       sizeof(MVPPushConstants), sizeof(MaterialPushConstants),
                       &matPc);
    cmd->DrawIndexed(submesh.indexCount, submesh.firstIndex, 0);
  }
}

} // namespace Rodan
