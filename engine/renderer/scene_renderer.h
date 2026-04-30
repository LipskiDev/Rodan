#pragma once

#include "graphics/material_resource.h"
#include "renderer/mesh_renderer.h"
#include "rhi/rhi_command_list.h"
#include "rhi/rhi_handles.h"
#include "rhi/rhi_pipeline.h"
#include "scene/camera.h"
#include "scene/static_mesh_instance.h"
#include <renderer/graph_renderer.h>
#include <renderer/line_renderer.h>
namespace Rodan {

struct StaticMeshRenderItem {
  const StaticMeshInstance *instance = nullptr;
  const std::vector<MaterialResource> *materials = nullptr;
  const MaterialResource *materialOverride = nullptr;
  glm::mat4 world = glm::mat4(1.0f);
  uint32_t objectId = 0;
};

struct MeshPipelineKey {
  AlphaMode alphaMode = AlphaMode::Opaque;
  bool doubleSided = false;

  bool operator==(const MeshPipelineKey &other) const {
    return alphaMode == other.alphaMode && doubleSided == other.doubleSided;
  }
};

struct MeshPipelineKeyHasher {
  size_t operator()(const MeshPipelineKey &k) const noexcept {
    size_t h = 0;

    h ^= std::hash<int>()(static_cast<int>(k.alphaMode)) + 0x9e3779b9 +
         (h << 6) + (h >> 2);

    h ^= std::hash<bool>()(k.doubleSided) + 0x9e3779b9 + (h << 6) + (h >> 2);

    return h;
  }
};

class SceneRenderer {
public:
  void Initialize(IDevice *device, SwapchainHandle swapchain,
                  Format colorFormat, Format depthFormat,
                  DescriptorSetLayoutHandle materialLayout);
  void Shutdown(IDevice *device);
  void Render(ICommandList &cmd, const Camera &camera);

  void SubmitStaticMesh(StaticMeshRenderItem item);

private:
  PipelineHandle GetOrCreatePipeline(const MeshPipelineKey &key);

private:
  IDevice *device_ = nullptr;
  SwapchainHandle swapchain_;

  Format colorFormat_ = Format::Undefined;
  Format depthFormat_ = Format::Undefined;

  ShaderHandle staticMeshVS_;
  ShaderHandle staticMeshFS_;

  DescriptorSetLayoutHandle materialLayout_{};

  MeshRenderer meshRenderer_;

  std::unique_ptr<Debug::GraphRenderer> graphRenderer_;
  std::unique_ptr<Debug::LineRenderer3D> lineRenderer3D_;
  std::unique_ptr<Debug::LineRenderer2D> lineRenderer2D_;

  std::vector<StaticMeshRenderItem> staticMeshes_;

  std::unordered_map<MeshPipelineKey, PipelineHandle, MeshPipelineKeyHasher>
      pipelines_;
};

} // namespace Rodan
