#include "renderer/mesh_renderer.h"
#include "graphics/mesh_resource.h"
#include "graphics/shaders_types.h"

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

void MeshRenderer::DrawSubmesh(ICommandList *cmd,
                               const StaticMeshInstance &instance,
                               const Submesh &submesh,
                               const MaterialResource *material,
                               PipelineHandle pipeline) {
  if (!cmd || !instance.mesh) {
    return;
  }

  const MeshResource &mesh = *instance.mesh;

  cmd->BindPipeline(pipeline);

  cmd->BindVertexBuffer(0, mesh.vertexBuffer, 0);
  cmd->BindIndexBuffer(mesh.indexBuffer, IndexType::U32, 0);

  MaterialPushConstants matPc{};

  if (material) {
    cmd->BindDescriptorSet(pipeline, 0, material->descriptorSet);

    matPc.baseColorFactor = material->baseColorFactor;
    matPc.metallicFactor = material->metallicFactor;
    matPc.roughnessFactor = material->roughnessFactor;
    matPc.hasMaterial = 1;
    matPc.alphaMode = static_cast<int>(material->alphaMode);
    matPc.alphaCutoff = material->alphaCutoff;
  } else {
    matPc.baseColorFactor = glm::vec4(1.0f);
    matPc.metallicFactor = 0.0f;
    matPc.roughnessFactor = 1.0f;
    matPc.hasMaterial = 0;
    matPc.alphaMode = static_cast<int>(AlphaMode::Opaque);
    matPc.alphaCutoff = 0.5f;
  }

  cmd->PushConstants(ShaderStage::Vertex | ShaderStage::Fragment,
                     sizeof(MVPPushConstants), sizeof(MaterialPushConstants),
                     &matPc);

  cmd->DrawIndexed(submesh.indexCount, submesh.firstIndex, 0);
}

} // namespace Rodan
