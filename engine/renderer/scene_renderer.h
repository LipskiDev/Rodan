#pragma once

#include "core/frame_render_context.h"
#include "graphics/environment/environment_map.h"
#include "graphics/environment/ibl_baker.h"
#include "graphics/material_resource.h"
#include "graphics/mesh_resource.h"
#include "renderer/mesh_renderer.h"
#include "renderer/passes/skybox_pass.h"
#include "renderer/render_graph/render_graph.h"
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

constexpr uint32_t k_MaxMaterials = 1024;

struct alignas(16) FrameDataGPU {
  glm::mat4 view;
  glm::mat4 proj;
  glm::mat4 lightViewProj;

  glm::vec4 lightDirection;
  glm::vec4 lightColor;

  glm::vec2 viewportSize;

  float lightIntensity;
  int shadowsEnabled;
  int showMode;
  float _pad0;
};

struct alignas(16) MaterialDataGPU {
  glm::vec4 baseColorFactor;

  float metallicFactor;
  float roughnessFactor;
  float alphaCutoff;
  int alphaMode;

  int hasMaterial;
  float transmissionFactor;
  float thicknessFactor;
  float ior;
  float clearcoatFactor;
  float clearcoatRoughnessFactor;
  float pad0_;
  float pad1_;
  glm::vec3 emissiveFactor;
  float emissiveStrength;

  glm::vec4 attenuationColorDistance;
};

struct StaticMeshRenderItem {
  const MeshResource *mesh = nullptr;
  const MaterialResource *material = nullptr;
  MaterialHandle materialHandle{};
  const Submesh *submesh = nullptr;

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
  void LoadEnvironment(IDevice *device, const std::string &path);

private:
  PipelineHandle GetOrCreatePipeline(const MeshPipelineKey &key);
  PipelineHandle GetOrCreateShadowPipeline();
  PipelineHandle GetOrCreateTransmissionPipeline();
  PipelineHandle GetOrCreateTonemappingPipeline();

  void RenderShadowMaps(ICommandList &cmd, const RenderWorld &world);
  void BuildStaticMeshRenderList(const RenderWorld &world);
  void RenderOpaqueMeshes(ICommandList &cmd, const Camera &camera,
                          DebugContext dbgCtx, DescriptorSetHandle set);
  void RenderTransmissionMeshes(ICommandList &cmd, const Camera &camera,
                                DebugContext dbgCtx);

  void RenderAlphaBlendMeshes(ICommandList &cmd, const Camera &camera,
                              DebugContext dbgCtx);
  void RenderStaticMeshes(ICommandList &cmd, const Camera &camera,
                          DebugContext dbgCtx,
                          const std::vector<StaticMeshRenderItem> &items,
                          DescriptorSetHandle sceneSet);

  void RenderPostProcessingEffect(ICommandList &cmd,
                                  const FrameRenderContext &frame,
                                  const DebugContext &dbgCtx);

  void RenderDebug(ICommandList &cmd, const RenderWorld &world,
                   const Camera &camera, DebugContext dbgCtx);

  void RenderOpaquePass(ICommandList &cmd, const FrameRenderContext &frame,
                        const Camera &camera, const DebugContext &dbgCtx);

  void RenderMainPass(ICommandList &cmd, const RenderWorld &world,
                      const FrameRenderContext &frame, const Camera &camera,
                      const DebugContext &dbgCtx);

  void RenderTonemappingPass(ICommandList &cmd, const RenderWorld &world,
                             const FrameRenderContext &frame,
                             const Camera &camera, const DebugContext &dbgCtx);

  void RenderUIPass(ICommandList &cmd, const FrameRenderContext &frame);

  void BeginMainPass(ICommandList &cmd, const FrameRenderContext &frame,
                     const Camera &camera, const DebugContext &dbgCtx);

  void BeginOpaquePass(ICommandList &cmd, const FrameRenderContext &frame,
                       const Camera &camera, const DebugContext dbgCtx);

  void BeginTonemappingPass(ICommandList &cmd, const FrameRenderContext &frame,
                            const Camera &camera, const DebugContext &dbgCtx);

  void EndMainPass(ICommandList &cmd);
  void EndOpaquePass(ICommandList &cmd);
  void EndTonemappingPass(ICommandList &cmd);

  void EnsureOpaqueSceneTarget(const FrameRenderContext &frame);
  void EnsureFinalSceneTarget(const FrameRenderContext &frame);
  void UpdateOpaqueSceneDescriptor();
  void UpdateFinalSceneDescriptor();

  void UploadMaterialBuffer(ICommandList &command, const RenderWorld &world);

private:
  IDevice *device_ = nullptr;
  SwapchainHandle swapchain_;

  Format colorFormat_ = Format::Undefined;
  Format depthFormat_ = Format::Undefined;

  ShaderHandle staticMeshVS_;
  ShaderHandle staticMeshFS_;

  ShaderHandle shadowVS_;
  ShaderHandle shadowFS_;

  ShaderHandle postProcessingVS_;
  ShaderHandle tonemappingFS_;

  PipelineHandle transmissionPipeline_{};
  PipelineHandle tonemappingPipeline_{};

  DescriptorSetLayoutHandle materialLayout_{};
  DescriptorSetLayoutHandle frameLayout_{};

  DescriptorPoolHandle postProcessingDescriptorPool_{};
  DescriptorSetLayoutHandle postProcessingLayout_{};
  DescriptorSetHandle postProcessingSet_{};

  DescriptorPoolHandle frameDescriptorPool_{};
  DescriptorSetHandle frameSet_{};

  DescriptorSetLayoutHandle opaqueSceneLayout_{};
  DescriptorPoolHandle opaqueScenePool_{};
  DescriptorSetHandle opaqueSceneSet_{};
  DescriptorSetHandle dummmyOpaqueSceneSet_{};

  BufferHandle frameUBO_;
  BufferHandle materialBuffer_;

  MeshRenderer meshRenderer_;

  std::unique_ptr<Debug::GraphRenderer> graphRenderer_;
  std::unique_ptr<Debug::LineRenderer3D> lineRenderer3D_;
  std::unique_ptr<Debug::LineRenderer2D> lineRenderer2D_;

  std::vector<StaticMeshRenderItem> staticMeshes_;

  std::vector<StaticMeshRenderItem> opaques_;
  std::vector<StaticMeshRenderItem> transmissions_;
  std::vector<StaticMeshRenderItem> alphaBlends_;

  std::unordered_map<MeshPipelineKey, PipelineHandle, MeshPipelineKeyHasher>
      pipelines_;

  // Only one shadow map as of right now
  ShadowMapResources directionalShadow_;

  SkyboxPass skyboxPass_;
  std::shared_ptr<EnvironmentMap> environment_;

  std::unique_ptr<IBLBaker> iblBaker_;
  IBLResources iblResources_;
  bool iblReady_ = false;

  struct OpaqueSceneFallback {
    Texture dummy{};
  };

  OpaqueSceneFallback opaqueSceneFallback_;

  bool recreated = false;

  std::unordered_map<uint32_t, uint32_t> materialGpuIndex_;

  RenderGraph graph_;
};

} // namespace Rodan
