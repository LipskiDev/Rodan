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
  if (!cmd) {
    return;
  }

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
