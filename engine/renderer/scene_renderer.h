#pragma once

#include "core/frame_render_context.h"
#include "graphics/environment/environment_map.h"
#include "graphics/environment/ibl_baker.h"
#include "graphics/material_resource.h"
#include "graphics/mesh_resource.h"
#include "renderer/mesh_renderer.h"
#include "renderer/passes/skybox_pass.h"
#include "renderer/render_graph/render_graph.h"
#include "rhi/command_list.h"
#include "rhi/handles.h"
#include "rhi/types.h"
#include "scene/camera.h"
#include "scene/render_world.h"
#include <renderer/graph_renderer.h>
#include <renderer/line_renderer.h>
#include <renderer/mesh_renderer.h>

#include <scene/render_world.h>
#include <limits>

namespace Rodan {

constexpr uint32_t k_MaxMaterials = 1024;

constexpr uint32_t k_MaxShadowCascades = 8;

constexpr uint32_t k_MaxGpuObjects = 500'000;
constexpr uint32_t k_MaxGpuDraws = 1'000'000;

struct DirectionalShadowSettings {
    uint32_t cascadeCount = 4;
    float maxDistance = 250.0f;

    std::array<float, k_MaxShadowCascades - 1> splits = {
        0.06f,
        0.18f,
        0.44f,
        0.58f,
        0.70f,
        0.82f,
        0.92f,
    };
};

struct alignas(16) CascadeGPU {
    glm::mat4 lightViewProj;
    glm::vec4 splitData; // x = far Distance
};

struct alignas(16) FrameDataGPU {
  glm::mat4 view;
  glm::mat4 proj;

  std::array<CascadeGPU, k_MaxShadowCascades> cascades;
  uint32_t cascadeCount;
  uint32_t _cascadePad[3];

  glm::vec4 lightDirection;
  glm::vec4 lightColor;

  glm::vec2 viewportSize;

  float lightIntensity;
  int shadowsEnabled;
  int showMode;
  float _pad0;
};

static_assert(sizeof(CascadeGPU) == 80);
static_assert(offsetof(FrameDataGPU, lightDirection) % 16 == 0);
static_assert(offsetof(FrameDataGPU, lightColor) % 16 == 0);
static_assert(sizeof(FrameDataGPU) % 16 == 0);

struct alignas(16) TextureTransformDataGPU {
    alignas(16) glm::vec2 offset = glm::vec2{ 0, 0 }; // 0
    glm::vec2 scale{ 1,1 }; // 8
    float rotation{ 0 }; // 16
    int texCoord{ 0 }; // 20

    TextureTransformDataGPU() {}

    TextureTransformDataGPU(TextureTransformation textureTransformation) {
        offset = textureTransformation.offset;
        scale = textureTransformation.scale;
        rotation = textureTransformation.rotation;
        texCoord = textureTransformation.texCoord;
    }
}; // size 32

static_assert(offsetof(TextureTransformDataGPU, offset) == 0);
static_assert(offsetof(TextureTransformDataGPU, scale) == 8);
static_assert(offsetof(TextureTransformDataGPU, rotation) == 16);
static_assert(offsetof(TextureTransformDataGPU, texCoord) == 20);


static_assert(sizeof(TextureTransformDataGPU) == 32);

struct alignas(16) MaterialDataGPU {
    alignas(16) glm::vec4 baseColorFactor;

    int32_t baseColorTextureIndex;
    float metallicFactor;
    float roughnessFactor;
    int32_t metallicRoughnessTextureIndex;

    float alphaCutoff;
    int32_t alphaMode;
    int32_t hasMaterial;
    float transmissionFactor;

    float thicknessFactor;
    int32_t thicknessTextureIndex;
    float ior;
    float clearcoatFactor;

    int32_t clearcoatTextureIndex;
    float clearcoatRoughnessFactor;
    int32_t clearcoatRoughnessTextureIndex;
    int32_t clearcoatNormalTextureIndex;

    int32_t normalTextureIndex;
    int32_t occlusionTextureIndex;
    int32_t transmissionTextureIndex;
    int32_t pad0_;

    alignas(16) glm::vec3 emissiveFactor;
    float emissiveStrength;

    int32_t emissiveTextureIndex;
    uint32_t useUnlit;
    uint32_t pad1_;
    uint32_t pad2_;

    alignas(16) glm::vec4 attenuationColorDistance;

    TextureTransformDataGPU baseColorTextureTransformation{};              // 144
    TextureTransformDataGPU normalTextureTransformation{};                 // 176
    TextureTransformDataGPU metallicRoughnessTextureTransformation{};      // 208
    TextureTransformDataGPU thicknessTextureTransformation{};              // 240
    TextureTransformDataGPU clearcoatTextureTransformation{};              // 272
    TextureTransformDataGPU clearcoatRoughnessTextureTransformation{};     // 304
    TextureTransformDataGPU clearcoatNormalTextureTransformation{};        // 336
    TextureTransformDataGPU occlusionTextureTransformation{};              // 368
    TextureTransformDataGPU transmissionTextureTransformation{};           // 400
    TextureTransformDataGPU emissiveTextureTransformation{};               // 432
};

static_assert(alignof(MaterialDataGPU) == 16);
static_assert(sizeof(MaterialDataGPU) == 464);

static_assert(offsetof(MaterialDataGPU, normalTextureIndex) == 80);
static_assert(offsetof(MaterialDataGPU, emissiveFactor) == 96);
static_assert(offsetof(MaterialDataGPU, attenuationColorDistance) == 128);

static_assert(
    offsetof(MaterialDataGPU, baseColorTextureTransformation) == 144);
static_assert(
    offsetof(MaterialDataGPU, normalTextureTransformation) == 176);
static_assert(
    offsetof(MaterialDataGPU, metallicRoughnessTextureTransformation) == 208);
static_assert(
    offsetof(MaterialDataGPU, thicknessTextureTransformation) == 240);
static_assert(
    offsetof(MaterialDataGPU, clearcoatTextureTransformation) == 272);
static_assert(
    offsetof(MaterialDataGPU, clearcoatRoughnessTextureTransformation) == 304);
static_assert(
    offsetof(MaterialDataGPU, clearcoatNormalTextureTransformation) == 336);
static_assert(
    offsetof(MaterialDataGPU, occlusionTextureTransformation) == 368);
static_assert(
    offsetof(MaterialDataGPU, transmissionTextureTransformation) == 400);
static_assert(
    offsetof(MaterialDataGPU, emissiveTextureTransformation) == 432);

struct StaticMeshRenderItem {
  const MeshResource *mesh = nullptr;
  const MaterialResource *material = nullptr;
  MaterialHandle materialHandle{};
  const Submesh *submesh = nullptr;

  const MaterialResource *materialOverride = nullptr;

  Transform localTransform;
  Transform worldTransform;
  uint32_t objectId = 0;
  uint32_t drawDataIndex;
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

struct GpuBatchKey {
    MeshPipelineKey pipeline;

    BufferHandle vertexBuffer;
    BufferHandle indexBuffer;

    IndexType indexType;

    bool operator==(const GpuBatchKey& other) const noexcept {
        return pipeline == other.pipeline &&
               vertexBuffer.id == other.vertexBuffer.id &&
               indexBuffer.id == other.indexBuffer.id &&
               indexType == other.indexType;
    }
};

struct GpuBatchKeyHasher {
    size_t operator()(const GpuBatchKey& k) const noexcept {
        const auto hashCombine = [](size_t seed, size_t value) {
            return seed ^ (value + 0x9e3779b9u + (seed << 6) + (seed >> 2));
        };

        size_t h = MeshPipelineKeyHasher{}(k.pipeline);
        h = hashCombine(h, std::hash<uint32_t>{}(k.vertexBuffer.id));
        h = hashCombine(h, std::hash<uint32_t>{}(k.indexBuffer.id));
        h = hashCombine(h, std::hash<int>{}(static_cast<int>(k.indexType)));
        return h;
    }
};

struct GpuBatchDraw {
    uint32_t drawDataIndex;
    uint32_t indexCount;
    uint32_t firstIndex;
    int32_t vertexOffset;
};

struct GpuBatch {
    GpuBatchKey key;
    std::vector<GpuBatchDraw> draws;
    uint32_t indirectCommandOffset = 0;
    uint32_t indirectCommandCount = 0;
};

struct ShadowMapResources {
  ImageHandle image;
  std::vector<ImageViewHandle> views;
  ImageViewHandle arrayView;
  SamplerHandle sampler;
  PipelineHandle pipeline;
  std::vector<glm::mat4> lightViewProjs;
  uint32_t cascadeCount = 4;
  std::vector<float> cascades = { 0.015f, 0.05f, 0.2f };


  glm::vec3 direction;
  glm::vec3 color;
  float intensity;

  int enabled = 1;

  uint32_t resolution = 2048;
};

enum class DrawMode {
  Final,
  BaseColor,
  Normal,
  MetallicRoughness,
  Tangent,
  Occlusion,
  ShadowCascades
};

struct DebugContext {
  bool drawMeshBounds;
  bool drawSceneBounds;
  bool drawLightDirection;
  DrawMode mode;
};

struct GpuObjectSlot {
    uint32_t objectId;
    uint32_t generation;
    bool occupied;
};

class SceneRenderer {
public:
  void Initialize(IDevice *device, SwapchainHandle swapchain,
                  Format colorFormat, Format depthFormat,
                  BindingLayoutHandle materialLayout);
  void Shutdown(IDevice *device);
  void Render(ICommandList &cmd, const RenderWorld &world, Camera &camera,
              const FrameRenderContext &frame,
              DebugContext dbgCtx = {false, false, false});

  void SubmitStaticMesh(StaticMeshRenderItem item);
  void LoadEnvironment(IDevice *device, const std::string &path);
  DirectionalShadowSettings& GetShadowSettings();

private:
  PipelineHandle GetOrCreatePipeline(const MeshPipelineKey &key);
  PipelineHandle GetOrCreateShadowPipeline();
  PipelineHandle GetOrCreateTransmissionPipeline();
  PipelineHandle GetOrCreateTonemappingPipeline();

  void RenderShadowMaps(ICommandList &cmd, const RenderWorld &world, const Camera& camera);
  void BuildStaticMeshRenderList(const RenderWorld &world);
  void RenderOpaqueMeshes(ICommandList &cmd, const Camera &camera,
                          DebugContext dbgCtx, BindingSetHandle set);
  void RenderTransmissionMeshes(ICommandList &cmd, const Camera &camera,
                                DebugContext dbgCtx);

  void RenderAlphaBlendMeshes(ICommandList &cmd, const Camera &camera,
                              DebugContext dbgCtx);
  void RenderStaticMeshesDirect(
      ICommandList &cmd, const Camera &camera, DebugContext dbgCtx,
      const std::vector<StaticMeshRenderItem> &items,
      BindingSetHandle sceneSet);
  void RenderStaticMeshesIndirect(ICommandList &cmd,
                                  BindingSetHandle sceneSet);

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

  void UpdateOpaqueSceneDescriptor();
  void UpdateFinalSceneDescriptor();

  void UploadMaterialBuffer(ICommandList &command, const RenderWorld &world);

  void BuildGpuSceneData(const RenderWorld& world);
  void BuildOpaqueGpuBatch(const StaticMeshRenderItem& item);
  GpuBatch& FindOrCreateBatch(const GpuBatchKey& batchKey);

  GPUSceneObject ConvertToGpuObject(const RenderObject &object,
                                    const MeshResource &mesh) const;
  void UploadIfChanged(uint32_t slot, const GPUSceneObject& gpuObject);
  void UploadGpuData();

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

  BindingLayoutHandle materialLayout_{};
  BindingLayoutHandle frameLayout_{};

  BindingPoolHandle postProcessingBindingPool_{};
  BindingLayoutHandle postProcessingLayout_{};
  BindingSetHandle postProcessingSet_{};

  BindingPoolHandle frameBindingPool_{};
  BindingSetHandle frameSet_{};

  BindingLayoutHandle objectDataLayout_{};
  BindingPoolHandle objectDataBindingPool_{};
  BindingSetHandle objectDataSet_{};

  BindingLayoutHandle opaqueSceneLayout_{};
  BindingPoolHandle opaqueScenePool_{};
  BindingSetHandle opaqueSceneSet_{};
  BindingSetHandle dummmyOpaqueSceneSet_{};

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
  DirectionalShadowSettings directionalShadowSettings_;
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

  std::vector<GpuObjectSlot> slots_;
  std::vector<uint32_t> freeSlots_;
  std::vector<GPUSceneObject> gpuObjectsCpu_;
  std::vector<GPUDrawData> gpuDrawsCpu_;
  std::vector<Velos::RHI::DrawIndexedIndirectCommand> indirectCommandsCpu_;
  std::vector<uint32_t> dirtyGpuObjectSlots_;
  std::unordered_map<uint32_t, uint32_t> objectToSlot_;

  BufferHandle gpuObjectBuffer_;
  BufferHandle gpuDrawBuffer_;
  BufferHandle indirectCommands_;

  std::unordered_map<GpuBatchKey, GpuBatch, GpuBatchKeyHasher> gpuBatches_{};

  const RenderWorld *cachedRenderWorld_ = nullptr;
  uint64_t cachedStructureRevision_ = std::numeric_limits<uint64_t>::max();
  uint64_t cachedObjectRevision_ = std::numeric_limits<uint64_t>::max();
};

} // namespace Rodan
