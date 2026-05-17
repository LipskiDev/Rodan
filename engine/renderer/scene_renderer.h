#pragma once

#include "core/frame_render_context.h"
#include "graphics/environment/environment_map.h"
#include "graphics/environment/ibl_baker.h"
#include "graphics/material_resource.h"
#include "graphics/mesh_resource.h"
#include "renderer/mesh_renderer.h"
#include "renderer/passes/skybox_pass.h"
#include "rhi/rhi_command_list.h"
#include "rhi/rhi_handles.h"
#include "rhi/rhi_types.h"
#include "scene/camera.h"
#include "scene/render_world.h"
#include <renderer/graph_renderer.h>
#include <renderer/line_renderer.h>
#include <renderer/mesh_renderer.h>

#include <scene/render_world.h>

namespace Rodan {

struct alignas(16) FrameDataGPU {
  glm::mat4 view;
  glm::mat4 proj;
  glm::mat4 lightViewProj;

  glm::vec3 lightDirection;
  float _padD;
  glm::vec3 lightColor;
  float _padC;

  float lightIntensity;
  bool renderShadows;
  int showMode;
  float _pad1;
};

struct StaticMeshRenderItem {
  const MeshResource *mesh = nullptr;
  std::vector<const MaterialResource *> materials;

  const MaterialResource *materialOverride = nullptr;

  Transform localTransform;
  Transform worldTransform;
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

struct ShadowMapResources {
  Texture texture;
  PipelineHandle pipeline;
  glm::mat4 lightViewProj;

  glm::vec3 direction;
  glm::vec3 color;
  float intensity;

  int enabled = 1;

  uint32_t resolution = 8 * 1024;
};

enum class DrawMode {
  Final,
  BaseColor,
  Normal,
  MetallicRoughness,
  Tangent,
  Occlusion
};

struct DebugContext {
  bool drawMeshBounds;
  bool drawSceneBounds;
  bool drawLightDirection;
  DrawMode mode;
};

class SceneRenderer {
public:
  void Initialize(IDevice *device, SwapchainHandle swapchain,
                  Format colorFormat, Format depthFormat,
                  DescriptorSetLayoutHandle materialLayout);
  void Shutdown(IDevice *device);
  void Render(ICommandList &cmd, const RenderWorld &world, const Camera &camera,
              const FrameRenderContext &frame,
              DebugContext dbgCtx = {false, false, false});

  void SubmitStaticMesh(StaticMeshRenderItem item);

private:
  PipelineHandle GetOrCreatePipeline(const MeshPipelineKey &key);
  PipelineHandle GetOrCreateShadowPipeline();

  void RenderShadowMaps(ICommandList &cmd, const RenderWorld &world);
  void BuildStaticMeshRenderList(const RenderWorld &world);
  void RenderStaticMeshes(ICommandList &cmd, const Camera &camera,
                          DebugContext dbgCtx);

  void RenderDebug(ICommandList &cmd, const RenderWorld &world,
                   const Camera &camera, DebugContext dbgCtx);

  void BeginMainPass(ICommandList &cmd, const FrameRenderContext &frame,
                     const Camera &camera, const DebugContext &dbgCtx);

  void EndMainPass(ICommandList &cmd);

  void EnsureShadowMapReadable(ICommandList &cmd);

private:
  IDevice *device_ = nullptr;
  SwapchainHandle swapchain_;

  Format colorFormat_ = Format::Undefined;
  Format depthFormat_ = Format::Undefined;

  ImageLayout shadowLayout_ = ImageLayout::Undefined;

  ShaderHandle staticMeshVS_;
  ShaderHandle staticMeshFS_;

  ShaderHandle shadowVS_;
  ShaderHandle shadowFS_;

  DescriptorSetLayoutHandle materialLayout_{};
  DescriptorSetLayoutHandle frameLayout_{};

  DescriptorPoolHandle frameDescriptorPool_{};
  DescriptorSetHandle frameSet_{};

  BufferHandle frameUBO_;

  MeshRenderer meshRenderer_;

  std::unique_ptr<Debug::GraphRenderer> graphRenderer_;
  std::unique_ptr<Debug::LineRenderer3D> lineRenderer3D_;
  std::unique_ptr<Debug::LineRenderer2D> lineRenderer2D_;

  std::vector<StaticMeshRenderItem> staticMeshes_;

  std::unordered_map<MeshPipelineKey, PipelineHandle, MeshPipelineKeyHasher>
      pipelines_;

  // Only one shadow map as of right now
  ShadowMapResources directionalShadow_;

  SkyboxPass skyboxPass_;
  std::shared_ptr<EnvironmentMap> environment_;

  std::unique_ptr<IBLBaker> iblBaker_;
  IBLResources iblResources_;
  bool iblReady_ = false;
};

} // namespace Rodan
