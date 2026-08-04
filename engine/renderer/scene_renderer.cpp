#include "glm/ext/matrix_clip_space.hpp"
#include "glm/ext/matrix_transform.hpp"
#include "graphics/environment/environment_map.h"
#include "graphics/environment/ibl_baker.h"
#include "graphics/material_types.h"
#include "graphics/mesh_resource.h"
#include "graphics/shader_cache.h"
#include "graphics/shaders_types.h"
#include "graphics/texture.h"
#include "renderer/render_graph/render_graph_builder.h"
#include "rhi/command_list.h"
#include "rhi/handles.h"
#include "rhi/pipeline.h"
#include "rhi/resources.h"
#include "rhi/types.h"
#include "scene/handles.h"
#include "scene/render_world.h"
#include "tracy/Tracy.hpp"
#include <core/path.h>
#include <cstring>
#include <iostream>
#include <renderer/scene_renderer.h>
#include <stdexcept>
#include <string>
#include <graphics/texture_registry.h>

namespace Rodan {
void SceneRenderer::Initialize(IDevice *device, SwapchainHandle swapchain,
                               Format colorFormat, Format depthFormat,
                               BindingLayoutHandle materialLayout) {

  device_ = device;
  swapchain_ = swapchain;
  colorFormat_ = colorFormat;
  depthFormat_ = depthFormat;
  materialLayout_ = materialLayout;

  graphRenderer_ = std::make_unique<Debug::GraphRenderer>("Frame Graph");
  lineRenderer3D_ = std::make_unique<Debug::LineRenderer3D>(device);
  lineRenderer2D_ = std::make_unique<Debug::LineRenderer2D>();

  auto vertSpv = ShaderCache::LoadOrCompile({
      .path = Velos::Path::Resolve("assets/shaders/static_mesh.vert").string(),
      .stage = ShaderStage::Vertex,
      .entryPoint = "main",
  });

  auto fragSpv = ShaderCache::LoadOrCompile({
      .path = Velos::Path::Resolve("assets/shaders/static_mesh.frag").string(),
      .stage = ShaderStage::Fragment,
      .entryPoint = "main",
  });

  staticMeshVS_ = device_->CreateShader({
      .stage = ShaderStage::Vertex,
      .bytecode = vertSpv.spirv.data(),
      .bytecodeSize =
          static_cast<uint64_t>(vertSpv.spirv.size() * sizeof(std::uint32_t)),
      .entryPoint = "main",
      .reflection = vertSpv.reflection,
      .debugName = "SceneRenderer Static Mesh VS",
  });

  staticMeshFS_ = device_->CreateShader({
      .stage = ShaderStage::Fragment,
      .bytecode = fragSpv.spirv.data(),
      .bytecodeSize =
          static_cast<uint64_t>(fragSpv.spirv.size() * sizeof(std::uint32_t)),
      .entryPoint = "main",
      .reflection = fragSpv.reflection,
      .debugName = "SceneRenderer Static Mesh FS",
  });

  if (!staticMeshVS_.IsValid() || !staticMeshFS_.IsValid()) {
    throw std::runtime_error(
        "SceneRenderer::Initialize: failed to create static mesh shaders");
  }

  auto depthVertSpv = ShaderCache::LoadOrCompile({
      .path = Velos::Path::Resolve("assets/shaders/shadow_depth.vert").string(),
      .stage = ShaderStage::Vertex,
      .entryPoint = "main",
  });

  auto depthFragSpv = ShaderCache::LoadOrCompile({
      .path = Velos::Path::Resolve("assets/shaders/shadow_depth.frag").string(),
      .stage = ShaderStage::Fragment,
      .entryPoint = "main",
  });

  shadowVS_ = device_->CreateShader({
      .stage = ShaderStage::Vertex,
      .bytecode = depthVertSpv.spirv.data(),
      .bytecodeSize = static_cast<uint64_t>(depthVertSpv.spirv.size() *
                                            sizeof(std::uint32_t)),
      .entryPoint = "main",
      .reflection = depthVertSpv.reflection,
      .debugName = "SceneRenderer Static Mesh VS",
  });

  shadowFS_ = device_->CreateShader({
      .stage = ShaderStage::Fragment,
      .bytecode = depthFragSpv.spirv.data(),
      .bytecodeSize = static_cast<uint64_t>(depthFragSpv.spirv.size() *
                                            sizeof(std::uint32_t)),
      .entryPoint = "main",
      .reflection = depthFragSpv.reflection,
      .debugName = "SceneRenderer Static Mesh FS",
  });

  if (!shadowVS_.IsValid() || !shadowFS_.IsValid()) {
    throw std::runtime_error(
        "SceneRenderer::Initialize: failed to create static mesh shaders");
  }

  vertSpv = ShaderCache::LoadOrCompile({
      .path =
          Velos::Path::Resolve("assets/shaders/postprocessing.vert").string(),
      .stage = ShaderStage::Vertex,
      .entryPoint = "main",
  });

  fragSpv = ShaderCache::LoadOrCompile({
      .path = Velos::Path::Resolve("assets/shaders/tonemapping.frag").string(),
      .stage = ShaderStage::Fragment,
      .entryPoint = "main",
  });

  postProcessingVS_ = device_->CreateShader({
      .stage = ShaderStage::Vertex,
      .bytecode = vertSpv.spirv.data(),
      .bytecodeSize =
          static_cast<uint64_t>(vertSpv.spirv.size() * sizeof(std::uint32_t)),
      .entryPoint = "main",
      .reflection = vertSpv.reflection,
      .debugName = "SceneRenderer Post Processing VS",
  });

  tonemappingFS_ = device_->CreateShader({
      .stage = ShaderStage::Fragment,
      .bytecode = fragSpv.spirv.data(),
      .bytecodeSize =
          static_cast<uint64_t>(fragSpv.spirv.size() * sizeof(std::uint32_t)),
      .entryPoint = "main",
      .reflection = fragSpv.reflection,
      .debugName = "SceneRenderer Tonemapping FS",
  });

  if (!postProcessingVS_.IsValid() || !tonemappingFS_.IsValid()) {
    throw std::runtime_error("SceneRenderer::Initialize: failed to create post "
                             "processing tonemapping shaders");
  }

  directionalShadow_.cascadeCount = std::clamp(
      directionalShadowSettings_.cascadeCount, 1u, k_MaxShadowCascades);
  directionalShadowSettings_.cascadeCount = directionalShadow_.cascadeCount;

  directionalShadow_.image = device->CreateImage({
      .width = directionalShadow_.resolution,
      .height = directionalShadow_.resolution,
      .arrayLayers = directionalShadow_.cascadeCount,
      .format = Format::D32_FLOAT,
      .usage = ImageUsage::DepthStencil | ImageUsage::Sampled,
      .debugName = "Directional Shadow Map",
  });

  directionalShadow_.views.resize(directionalShadow_.cascadeCount);
  directionalShadow_.lightViewProjs.resize(directionalShadow_.cascadeCount);
  for (uint32_t i = 0; i < directionalShadow_.cascadeCount; i++) {
      directionalShadow_.views[i] = device->CreateImageView({
      .image = directionalShadow_.image,
      .format = Format::D32_FLOAT,
      .type = ImageViewType::View2D,
      .aspect = ImageAspect::Depth,
      .baseArrayLayer = i,
      .arrayLayerCount = 1,
          });
  }

  directionalShadow_.arrayView = device->CreateImageView({
    .image = directionalShadow_.image,
    .format = Format::D32_FLOAT,
    .type = ImageViewType::View2DArray,
    .aspect = ImageAspect::Depth,
    .baseArrayLayer = 0,
    .arrayLayerCount = directionalShadow_.cascadeCount,
      });

  directionalShadow_.sampler = device->CreateSampler({
      .minFilter = Filter::Linear,
      .magFilter = Filter::Linear,
      .addressU = SamplerAddressMode::ClampToEdge,
      .addressV = SamplerAddressMode::ClampToEdge,
      .addressW = SamplerAddressMode::ClampToEdge,
  });

  directionalShadow_.enabled = true;

  BindingDesc frameBindings[] = {
      {
          .binding = 0,
          .type = BindingType::CombinedImageSampler,
          .count = 1,
          .visibility = ShaderStage::Fragment,
      },
      {.binding = 1,
       .type = BindingType::UniformBuffer,
       .count = 1,
       .visibility = ShaderStage::Vertex | ShaderStage::Fragment},
      {.binding = 2,
       .type = BindingType::StorageBuffer,
       .count = 1,
       .visibility = ShaderStage::Fragment},
  };

  BindingDesc objectDataBindings[] = {
      {
          .binding = 0,
          .type = BindingType::StorageBuffer,
          .count = 1,
          .visibility = ShaderStage::Vertex | ShaderStage::Fragment
      },
      {
          .binding = 1,
          .type = BindingType::StorageBuffer,
          .count = 1,
          .visibility = ShaderStage::Vertex | ShaderStage::Fragment,
      },
  };

  BindingDesc environmentBindings[] = {
      {
          .binding = 0,
          .type = BindingType::CombinedImageSampler,
          .count = 1,
          .visibility = ShaderStage::Fragment,
      },
  };

  BindingPoolSize poolSizes[] = {
      {
          .type = BindingType::CombinedImageSampler,
          .count = 1,
      },
      {
          .type = BindingType::UniformBuffer,
          .count = 1,
      },
      {
          .type = BindingType::StorageBuffer,
          .count = 2,
      }};

  BindingPoolSize objectDataPoolSizes[] = {
      {
          .type = BindingType::StorageBuffer,
          .count = 2,
      },
  };

  objectDataBindingPool_ = device_->CreateBindingPool({
      .poolSizes = objectDataPoolSizes,
      .poolSizeCount = 1,
      .maxSets = 1,
  });

  frameBindingPool_ = device_->CreateBindingPool({
      .poolSizes = poolSizes,
      .poolSizeCount = 3,
      .maxSets = 1,
  });

  frameLayout_ = device_->CreateBindingLayout({
      .bindings = frameBindings,
      .bindingCount = 3,
      .debugName = "SceneRenderer Frame Descriptor Set Layout",
  });

  objectDataLayout_ = device_->CreateBindingLayout({
      .bindings = objectDataBindings,
      .bindingCount = 2,
      .debugName = "SceneRenderer Object Data Descriptor Set Layout",
      });

  BindingDesc postProcessingBindings[] = {
      {
          .binding = 0,
          .type = BindingType::CombinedImageSampler,
          .count = 1,
          .visibility = ShaderStage::Fragment,
      },
  };

  postProcessingLayout_ = device_->CreateBindingLayout({
      .bindings = postProcessingBindings,
      .bindingCount = 1,
      .debugName = "SceneRenderer Post Processing Descriptor Set Layout",
  });

  BindingPoolSize poolSizesPostProcessing[] = {
      {
          .type = BindingType::CombinedImageSampler,
          .count = 1,
      },
  };

  postProcessingBindingPool_ = device_->CreateBindingPool({
      .poolSizes = poolSizesPostProcessing,
      .poolSizeCount = 1,
      .maxSets = 1,
  });

  postProcessingSet_ = device_->AllocateBindingSet({
      .pool = postProcessingBindingPool_, .layout = postProcessingLayout_ });

  frameSet_ =
      device_->AllocateBindingSet({ .pool = frameBindingPool_, .layout = frameLayout_ });

  objectDataSet_ = device_->AllocateBindingSet({ .pool = objectDataBindingPool_, .layout = objectDataLayout_ });

  frameUBO_ = device_->CreateBuffer({.size = sizeof(FrameDataGPU),
                                     .usage = BufferUsage::Uniform,
                                     .memoryUsage = MemoryUsage::CPUToGPU,
                                     .debugName = "Scene Frame UBO"});

  materialBuffer_ = device_->CreateBuffer({
      .size = sizeof(MaterialDataGPU) * k_MaxMaterials,
      .usage = BufferUsage::Storage,
      .memoryUsage = MemoryUsage::CPUToGPU,
      .debugName = "Scene Material Buffer",
  });

  BindingDesc opaqueSceneBindings[] = {
      {
          .binding = 0,
          .type = BindingType::CombinedImageSampler,
          .count = 1,
          .visibility = ShaderStage::Fragment,
      },
  };

  opaqueSceneLayout_ = device_->CreateBindingLayout({
      .bindings = opaqueSceneBindings,
      .bindingCount = 1,
      .debugName = "Opaque Scene Descriptor Set Layout",
  });

  BindingPoolSize opaqueScenePoolSizes[] = {
      {
          .type = BindingType::CombinedImageSampler,
          .count = 2,
      },
  };

  opaqueScenePool_ = device_->CreateBindingPool({
      .poolSizes = opaqueScenePoolSizes,
      .poolSizeCount = 1,
      .maxSets = 2,
  });

  opaqueSceneSet_ =
      device_->AllocateBindingSet({ .pool = opaqueScenePool_, .layout = opaqueSceneLayout_ });

  dummmyOpaqueSceneSet_ =
      device_->AllocateBindingSet({ .pool = opaqueScenePool_, .layout = opaqueSceneLayout_ });

  auto uploadCtx = device_->CreateUploadContext(4 * 1024 * 1024);
  uploadCtx->Begin();

  const uint8_t whitePixel[] = {255, 255, 255, 255};

  opaqueSceneFallback_.dummy = CreateTexture2D(
      device_, uploadCtx.get(),
      {
          .width = 1,
          .height = 1,
          .format = Format::RGBA8_UNORM,
          .usage = ImageUsage::Sampled | ImageUsage::TransferDst,
          .minFilter = Filter::Nearest,
          .magFilter = Filter::Nearest,
          .addressU = SamplerAddressMode::ClampToEdge,
          .addressV = SamplerAddressMode::ClampToEdge,
          .addressW = SamplerAddressMode::ClampToEdge,
          .debugName = "Dummy Opaque Scene",
      },
      whitePixel, sizeof(whitePixel));

  uploadCtx->Flush();

  BindingImageInfo dummyImageInfo{};
  dummyImageInfo.imageView = opaqueSceneFallback_.dummy.view;
  dummyImageInfo.sampler = opaqueSceneFallback_.dummy.sampler;
  dummyImageInfo.imageLayout = ImageLayout::ShaderReadOnly;

  BindingImageInfo shadowImage{};
  shadowImage.imageView = directionalShadow_.arrayView;;
  shadowImage.sampler = directionalShadow_.sampler;
  shadowImage.imageLayout = ImageLayout::ShaderReadOnly;

  device->UpdateBindingSet({
      .dstSet = frameSet_,
      .binding = 0,
      .arrayElement = 0,
      .type = BindingType::CombinedImageSampler,
      .bufferInfo = nullptr,
      .imageInfo = &shadowImage,
      .descriptorCount = 1,
  });

  BindingBufferInfo frameBuffer{};
  frameBuffer.buffer = frameUBO_;
  frameBuffer.offset = 0;
  frameBuffer.range = sizeof(FrameDataGPU);

  device->UpdateBindingSet({
      .dstSet = frameSet_,
      .binding = 1,
      .type = BindingType::UniformBuffer,
      .bufferInfo = &frameBuffer,
      .descriptorCount = 1,
  });

  BindingBufferInfo materialBuffer{};
  materialBuffer.buffer = materialBuffer_;
  materialBuffer.offset = 0;
  materialBuffer.range = sizeof(MaterialDataGPU) * k_MaxMaterials;

  device_->UpdateBindingSet({
      .dstSet = frameSet_,
      .binding = 2,
      .type = BindingType::StorageBuffer,
      .bufferInfo = &materialBuffer,
      .descriptorCount = 1,
  });

  environment_ = EnvironmentMap::LoadHDR(
      device, Velos::Path::Resolve("assets/hdr/kloppenheim_02_puresky_4k.hdr"));

  skyboxPass_.Initialize(device, colorFormat,
                         environment_->GetBindingLayout());

  iblBaker_ = std::make_unique<IBLBaker>();
  iblBaker_->Initialize(device, environment_->GetBindingLayout());

  BufferDesc objectBufferDesc{};
  objectBufferDesc.size = sizeof(GPUSceneObject) * k_MaxGpuObjects;
  objectBufferDesc.usage = BufferUsage::Storage | BufferUsage::TransferDst;
  objectBufferDesc.memoryUsage = MemoryUsage::GPUOnly;
  objectBufferDesc.debugName = "GPU Driven Object Buffer";

  BufferDesc drawBufferDesc{};
  drawBufferDesc.size = sizeof(GPUDrawData) * k_MaxGpuDraws;
  drawBufferDesc.usage = BufferUsage::Storage | BufferUsage::TransferDst;
  drawBufferDesc.memoryUsage = MemoryUsage::GPUOnly;
  drawBufferDesc.debugName = "GPU Driven Draw Buffer";

  BufferDesc indirectCommandsBufferDesc{
    .size = sizeof(Velos::RHI::DrawIndexedIndirectCommand) * k_MaxGpuDraws,
    .usage = BufferUsage::Storage | BufferUsage::Indirect | BufferUsage::TransferDst,
    .memoryUsage = MemoryUsage::GPUOnly,
    .debugName = "GPU Driven Indirect Command Buffer",
  };

  gpuObjectBuffer_ = device_->CreateBuffer(objectBufferDesc);
  gpuDrawBuffer_ = device_->CreateBuffer(drawBufferDesc);
  indirectCommands_ = device_->CreateBuffer(indirectCommandsBufferDesc);
}

void SceneRenderer::Shutdown(IDevice *device) {
  // ReloadScene also drives the initial scene load. In that case there is no
  // renderer state to destroy yet.
  if (device_ == nullptr) {
    return;
  }

  if (device == nullptr) {
    device = device_;
  }

  device->DestroyBuffer(gpuObjectBuffer_);
  device->DestroyBuffer(gpuDrawBuffer_);
  device->DestroyBuffer(indirectCommands_);

  cachedRenderWorld_ = nullptr;
  cachedStructureRevision_ = std::numeric_limits<uint64_t>::max();
  cachedObjectRevision_ = std::numeric_limits<uint64_t>::max();

  graph_.Shutdown(*device);
  DestroyTexture(device, opaqueSceneFallback_.dummy);

  for (auto &[key, pipeline] : pipelines_) {
    device->DestroyPipeline(pipeline);
  }
  pipelines_.clear();

  if (tonemappingPipeline_) {
    device->DestroyPipeline(tonemappingPipeline_);
    tonemappingPipeline_ = {};
  }

  if (postProcessingVS_) {
    device->DestroyShader(postProcessingVS_);
    postProcessingVS_ = {};
  }

  if (tonemappingFS_) {
    device->DestroyShader(tonemappingFS_);
    tonemappingFS_ = {};
  }

  if (postProcessingBindingPool_.IsValid()) {
    device->DestroyBindingPool(postProcessingBindingPool_);
    postProcessingBindingPool_ = {};
    postProcessingSet_ = {};
  }

  if (postProcessingLayout_.IsValid()) {
    device->DestroyBindingLayout(postProcessingLayout_);
    postProcessingLayout_ = {};
  }

  DestroyTexture(device, iblResources_.prefilterTexture);

  for (ImageViewHandle handle : iblResources_.prefilterFaceMipViews) {
    if (handle.IsValid()) {
      device->DestroyImageView(handle);
    }
  }

  DestroyTexture(device, iblResources_.irradianceTexture);
  device_->DestroyBindingLayout(iblResources_.descriptorSetLayout);
  device_->DestroyBindingPool(iblResources_.descriptorPool);
  for (ImageViewHandle handle : iblResources_.irradianceFaceViews) {
    device->DestroyImageView(handle);
  }

  DestroyTexture(device, iblResources_.brdfLutTexture);
  device_->DestroyImageView(iblResources_.brdfLutView);

  iblBaker_->Shutdown(device);
  iblReady_ = false;

  skyboxPass_.Shutdown(device);

  if (environment_) {
    environment_->Destroy();
    environment_.reset();
  }

  if (directionalShadow_.pipeline) {
    device_->DestroyPipeline(directionalShadow_.pipeline);
    directionalShadow_.pipeline = {};
  }

  if (staticMeshVS_) {
    device->DestroyShader(staticMeshVS_);
    staticMeshVS_ = {};
  }

  if (staticMeshFS_) {
    device->DestroyShader(staticMeshFS_);
    staticMeshFS_ = {};
  }

  if (shadowVS_) {
    device->DestroyShader(shadowVS_);
    shadowVS_ = {};
  }

  if (shadowFS_) {
    device->DestroyShader(shadowFS_);
    shadowFS_ = {};
  }

  if (transmissionPipeline_) {
    device_->DestroyPipeline(transmissionPipeline_);
    transmissionPipeline_ = {};
  }

  if (opaqueScenePool_.IsValid()) {
    device->DestroyBindingPool(opaqueScenePool_);
    opaqueScenePool_ = {};
    opaqueSceneSet_ = {};
  }

  if (opaqueSceneLayout_.IsValid()) {
    device->DestroyBindingLayout(opaqueSceneLayout_);
    opaqueSceneLayout_ = {};
  }

  device_->DestroySampler(directionalShadow_.sampler);
  for (ImageViewHandle view : directionalShadow_.views) {
    device_->DestroyImageView(view);
  }
  device_->DestroyImageView(directionalShadow_.arrayView);
  device_->DestroyImage(directionalShadow_.image);

  device->DestroyBuffer(frameUBO_);
  device->DestroyBuffer(materialBuffer_);
  device->DestroyBindingLayout(frameLayout_);
  device->DestroyBindingPool(frameBindingPool_);

  graphRenderer_.reset();
  lineRenderer3D_.reset();
  lineRenderer2D_.reset();

  staticMeshes_.clear();

  device_ = nullptr;
  swapchain_ = {};
}

void SceneRenderer::Render(ICommandList &cmd, const RenderWorld &world,
                           Camera &camera,
                           const FrameRenderContext &frame,
                           DebugContext dbgCtx) {
  graph_.Reset();
  const uint32_t cascadeCount = std::clamp(
      directionalShadowSettings_.cascadeCount, 1u,
      std::min(k_MaxShadowCascades,
               static_cast<uint32_t>(directionalShadow_.views.size())));
  directionalShadowSettings_.cascadeCount = cascadeCount;
  directionalShadow_.cascadeCount = cascadeCount;
  directionalShadow_.cascades.assign(
      directionalShadowSettings_.splits.begin(),
      directionalShadowSettings_.splits.begin() +
          (cascadeCount - 1));
  camera.SetCascades(cascadeCount, directionalShadow_.cascades,
                     directionalShadowSettings_.maxDistance);

  const uint32_t opaqueMipLevels =
      1u + static_cast<uint32_t>(std::floor(
               std::log2(std::max(frame.extent.width, frame.extent.height))));

  if (graph_.RegisterImage(
          *device_, "OpaqueScene",
          {
              .width = frame.extent.width,
              .height = frame.extent.height,
              .format = colorFormat_,
              .mipLevels = opaqueMipLevels,
              .arrayLayers = 1,
              .usage = ImageUsage::ColorAttachment | ImageUsage::Sampled |
                       ImageUsage::TransferSrc | ImageUsage::TransferDst,
              .aspect = ImageAspect::Color,
              .viewType = ImageViewType::View2D,
              .minFilter = Filter::Linear,
              .magFilter = Filter::Linear,
              .addressU = SamplerAddressMode::ClampToEdge,
              .addressV = SamplerAddressMode::ClampToEdge,
              .addressW = SamplerAddressMode::ClampToEdge,
              .debugName = "Opaque Scene Color",
          })) {
    UpdateOpaqueSceneDescriptor();
  }
  bool recreatedColor = graph_.RegisterImage(
      *device_, "FinalSceneColor",
      {
          .width = frame.extent.width,
          .height = frame.extent.height,
          .format = colorFormat_,
          .mipLevels = 1,
          .arrayLayers = 1,
          .usage = ImageUsage::ColorAttachment | ImageUsage::Sampled |
                   ImageUsage::TransferSrc | ImageUsage::TransferDst,
          .aspect = ImageAspect::Color,
          .viewType = ImageViewType::View2D,
          .debugName = "Final Scene Color",
      });

  bool recreatedDepth = graph_.RegisterImage(
      *device_, "FinalSceneDepth",
      {
          .width = frame.extent.width,
          .height = frame.extent.height,
          .format = depthFormat_,
          .mipLevels = 1,
          .arrayLayers = 1,
          .usage = ImageUsage::DepthStencil | ImageUsage::Sampled,
          .aspect = ImageAspect::Depth,
          .viewType = ImageViewType::View2D,
          .debugName = "Final Scene Depth",
      });

  if (recreatedColor || recreatedDepth) {
    UpdateFinalSceneDescriptor();
  }

  using Clock = std::chrono::high_resolution_clock;

  auto now = []() { return Clock::now(); };

  auto ms = [](Clock::time_point a, Clock::time_point b) {
    return std::chrono::duration<double, std::milli>(b - a).count();
  };

  const auto tFrameStart = now();

  Clock::time_point t0;
  Clock::time_point tTargets;
  Clock::time_point tBuildList;
  Clock::time_point tUploadMaterials;
  Clock::time_point tShadow;
  Clock::time_point tEnvUpload;
  Clock::time_point tIBL;
  Clock::time_point tOpaquePass;
  Clock::time_point tMips;
  Clock::time_point tMainPass;
  Clock::time_point tTonemap;
  Clock::time_point tUI;

  graph_.AddPass(
      "Upload Materials",
      [](RenderGraphBuilder &builder) {
        builder.StorageWrite("MaterialBuffer");
      },
      [&](ICommandList &cmd) {
        const bool newWorld = cachedRenderWorld_ != &world;
        const bool structureChanged =
            newWorld || cachedStructureRevision_ != world.GetStructureRevision();
        if (structureChanged) {
          UploadMaterialBuffer(cmd, world);
        }

        tUploadMaterials = now();
      });

  graph_.AddPass(
      "Build Static Mesh Render List", [](RenderGraphBuilder &) {},
      [&](ICommandList &) {
        const bool newWorld = cachedRenderWorld_ != &world;
        const bool structureChanged =
            newWorld || cachedStructureRevision_ != world.GetStructureRevision();
        const bool objectsChanged =
            newWorld || cachedObjectRevision_ != world.GetObjectRevision();

        if (structureChanged || objectsChanged) {
          BuildStaticMeshRenderList(world);
        }

        cachedRenderWorld_ = &world;
        cachedStructureRevision_ = world.GetStructureRevision();
        cachedObjectRevision_ = world.GetObjectRevision();

        tBuildList = now();
      });

  graph_.AddPass(
      "Shadow Maps",
      [&](RenderGraphBuilder &builder) {
        builder.WriteDepthAttachment("DirectionalShadowMap", SubresourceRange{.baseLayer = 0, .layerCount = directionalShadow_.cascadeCount });
      },
      [&](ICommandList &cmd) {
        RenderShadowMaps(cmd, world, camera);

        tShadow = now();
      });

  graph_.AddPass(
      "Environment Upload",
      [](RenderGraphBuilder &builder) { builder.CopyDst("EnvironmentMap"); },
      [&](ICommandList &cmd) {
        if (environment_ && environment_->NeedsUpload()) {
          environment_->RecordUpload(cmd);
        }

        tEnvUpload = now();
      });

  graph_.AddPass(
      "IBL Bake",
      [](RenderGraphBuilder &builder) {
        builder.ReadTexture("EnvironmentMap");
        builder.StorageWrite("IBLResources");
      },
      [&](ICommandList &cmd) {
        if (environment_ && !iblReady_) {
          iblResources_ = iblBaker_->BakeIrradiance(cmd, *environment_);
          iblReady_ = true;
        }

        tIBL = now();
      });

  graph_.AddPass(
      "Opaque Prepass For Transmission",
      [&](RenderGraphBuilder &builder) {
        builder.StorageRead("MaterialBuffer");
        builder.ReadTexture("DirectionalShadowMap", SubresourceRange{.baseLayer = 0, .layerCount = directionalShadow_.cascadeCount });
        builder.ReadTexture("IBLResources");

        builder.WriteColorAttachment("OpaqueScene", {
                                                        .baseMip = 0,
                                                        .mipCount = 1,
                                                    });
        builder.WriteDepthAttachment("FrameDepth");
      },
      [&](ICommandList &cmd) {
        if (!transmissions_.empty()) {
          RenderOpaquePass(cmd, frame, camera, dbgCtx);
        }

        tOpaquePass = now();
      });

  for (uint32_t i = 1; i < opaqueMipLevels; i++) {
    graph_.AddPass(
        "Generate Opaque Scene Mip" + std::to_string(i),
        [this, i](RenderGraphBuilder &builder) {
          builder.CopySrc("OpaqueScene", {.baseMip = i - 1, .mipCount = 1});
          builder.CopyDst("OpaqueScene", {.baseMip = i, .mipCount = 1});
        },
        [this, &frame, i](ICommandList &cmd) {
          if (!transmissions_.empty()) {
            const auto &opaqueScene = graph_.GetImage("OpaqueScene");
            cmd.BlitMip(opaqueScene.image, frame.extent.width,
                        frame.extent.height, i - 1, i, 1);
          }
        });
  }

  graph_.AddPass(
      "Main Pass",
      [this](RenderGraphBuilder &builder) {
        const auto &opaque = graph_.GetImage("OpaqueScene");

        builder.StorageRead("MaterialBuffer");
        builder.ReadTexture("DirectionalShadowMap", {
            .baseLayer = 0,
            .layerCount = directionalShadow_.cascadeCount
            });
        builder.ReadTexture("IBLResources");
        builder.ReadTexture("OpaqueScene",
                            {.baseMip = 0, .mipCount = opaque.mipLevels});

        builder.WriteColorAttachment("FinalSceneColor");
        builder.WriteDepthAttachment("FinalSceneDepth");
      },
      [&](ICommandList &cmd) {
        RenderMainPass(cmd, world, frame, camera, dbgCtx);

        tMainPass = now();
      });

  graph_.AddPass(
      "Tonemapping",
      [](RenderGraphBuilder &builder) {
        builder.ReadTexture("FinalSceneColor");
        builder.WriteColorAttachment("Backbuffer");
      },
      [&](ICommandList &cmd) {
        RenderTonemappingPass(cmd, world, frame, camera, dbgCtx);

        tTonemap = now();
      });

  graph_.AddPass(
      "UI",
      [](RenderGraphBuilder &builder) {
        builder.ReadColorAttachment("Backbuffer");
        builder.WriteColorAttachment("Backbuffer");
      },
      [&](ICommandList &cmd) {
        RenderUIPass(cmd, frame);

        tUI = now();
      });

  graph_.ImportImage("DirectionalShadowMap", directionalShadow_.image,
                     ImageAspect::Depth);

  const ResourceState backbufferState =
      frame.backbufferLayout == ImageLayout::Present ? ResourceState::Present
                                                     : ResourceState::Undefined;
  graph_.ImportImage("Backbuffer", frame.backbufferImage, ImageAspect::Color, 1,
                     1, frame.backbufferLayout, backbufferState, true);

  graph_.ImportImage("FrameDepth", frame.depthImage, ImageAspect::Depth);

  graph_.Compile();
  graph_.Execute(cmd);

  const auto tFrameEnd = now();

  static int frameCounter = 0;
  frameCounter++;

  if (frameCounter % 600 == 0) {
    std::cout << "\nCPU frame timings:\n"
              << "  Ensure targets:   " << ms(t0, tTargets) << " ms\n"
              << "  Build list:       " << ms(tTargets, tBuildList) << " ms\n"
              << "  Upload materials: " << ms(tBuildList, tUploadMaterials)
              << " ms\n"
              << "  Shadow maps:      " << ms(tUploadMaterials, tShadow)
              << " ms\n"
              << "  Env upload:       " << ms(tShadow, tEnvUpload) << " ms\n"
              << "  IBL bake:         " << ms(tEnvUpload, tIBL) << " ms\n"
              << "  Opaque pass:      " << ms(tIBL, tOpaquePass) << " ms\n"
              << "  Generate mips:    " << ms(tOpaquePass, tMips) << " ms\n"
              << "  Main pass:        " << ms(tMips, tMainPass) << " ms\n"
              << "  Tonemapping:      " << ms(tMainPass, tTonemap) << " ms\n"
              << "  UI pass:          " << ms(tTonemap, tUI) << " ms\n"
              << "  Total Render():   " << ms(tFrameStart, tFrameEnd)
              << " ms\n";
  }
}

void SceneRenderer::SubmitStaticMesh(StaticMeshRenderItem item) {
  staticMeshes_.push_back(item);
}

void SceneRenderer::LoadEnvironment(IDevice *device, const std::string &path) {
  if (!device) {
    return;
  }

  device->WaitIdle();

  iblReady_ = false;

  DestroyTexture(device, iblResources_.prefilterTexture);

  for (ImageViewHandle handle : iblResources_.prefilterFaceMipViews) {
    if (handle.IsValid()) {
      device->DestroyImageView(handle);
    }
  }

  DestroyTexture(device, iblResources_.irradianceTexture);

  for (ImageViewHandle handle : iblResources_.irradianceFaceViews) {
    if (handle.IsValid()) {
      device->DestroyImageView(handle);
    }
  }

  DestroyTexture(device, iblResources_.brdfLutTexture);
  device_->DestroyImageView(iblResources_.brdfLutView);

  if (iblResources_.descriptorSetLayout.IsValid()) {
    device->DestroyBindingLayout(iblResources_.descriptorSetLayout);
  }

  if (iblResources_.descriptorPool.IsValid()) {
    device->DestroyBindingPool(iblResources_.descriptorPool);
  }

  iblResources_ = {};

  if (environment_) {
    environment_->Destroy();
    environment_.reset();
  }

  environment_ = EnvironmentMap::LoadHDR(device, Velos::Path::Resolve(path));
}

DirectionalShadowSettings& SceneRenderer::GetShadowSettings()
{
    return directionalShadowSettings_;
}

PipelineHandle SceneRenderer::GetOrCreatePipeline(const MeshPipelineKey &key) {
  auto it = pipelines_.find(key);
  if (it != pipelines_.end()) {
    return it->second;
  }

  if (!device_) {
    throw std::runtime_error(
        "SceneRenderer::GetOrCreatePipeline: device is null");
  }

  if (!staticMeshVS_.IsValid() || !staticMeshFS_.IsValid()) {
    throw std::runtime_error(
        "SceneRenderer::GetOrCreatePipeline: mesh shaders are invalid");
  }

  GraphicsPipelineDesc desc{};

  desc.vertexShader = staticMeshVS_;
  desc.fragmentShader = staticMeshFS_;
  desc.topology = PrimitiveTopology::TriangleList;
  desc.colorFormat = colorFormat_;
  desc.debugName = "SceneRenderer.MeshPipeline";

  // Vertex layout
  desc.vertexLayouts = GetMeshVertexLayout();

  // Descriptor set layouts
  BindingLayoutHandle setLayouts[] = {
      GetTextureRegistry().GetBindingLayout(),
      frameLayout_,
      iblResources_.descriptorSetLayout,
      opaqueSceneLayout_,
      objectDataLayout_
  };

  desc.layout.descriptorSetLayouts = setLayouts;
  desc.layout.descriptorSetLayoutCount = 5;

  // Raster state
  desc.raster.cullBackFaces = true;
  desc.raster.frontFaceCCW = true;
  desc.raster.wireframe = false;

  // Depth state
  desc.depth.depthTestEnable = true;
  desc.depth.depthWriteEnable =
      key.alphaMode == AlphaMode::Blend ? false : true;
  desc.depth.depthFormat = depthFormat_;

  // Blend state
  switch (key.alphaMode) {
  case AlphaMode::Opaque:
  case AlphaMode::Mask:
    desc.blend.enable = false;
    break;
  case AlphaMode::Blend:
    desc.blend.enable = true;
    desc.blend.srcColor = BlendFactor::SrcAlpha;
    desc.blend.dstColor = BlendFactor::OneMinusSrcAlpha;
    desc.blend.colorOp = BlendOp::Add;

    desc.blend.srcAlpha = BlendFactor::One;
    desc.blend.dstAlpha = BlendFactor::OneMinusSrcAlpha;
    desc.blend.alphaOp = BlendOp::Add;
    break;
  }

  PipelineHandle pipeline = device_->CreateGraphicsPipeline(desc);
  if (!pipeline.IsValid()) {
    throw std::runtime_error("SceneRenderer::GetOrCreatePipeline: failed to "
                             "create graphics pipeline");
  }

  pipelines_.emplace(key, pipeline);
  return pipeline;
}

PipelineHandle SceneRenderer::GetOrCreateShadowPipeline() {
  if (directionalShadow_.pipeline.IsValid()) {
    return directionalShadow_.pipeline;
  }

  GraphicsPipelineDesc desc{};
  desc.vertexShader = shadowVS_;
  desc.fragmentShader = shadowFS_;
  desc.vertexLayouts = GetMeshVertexLayout();
  desc.topology = PrimitiveTopology::TriangleList;

  desc.colorFormat = Format::Undefined;
  desc.depth.depthTestEnable = true;
  desc.depth.depthWriteEnable = true;
  desc.depth.depthFormat = Format::D32_FLOAT;

  desc.raster.cullBackFaces = false;
  desc.raster.frontFaceCCW = true;

  directionalShadow_.pipeline = device_->CreateGraphicsPipeline(desc);
  return directionalShadow_.pipeline;
}

PipelineHandle SceneRenderer::GetOrCreateTransmissionPipeline() {
  if (transmissionPipeline_.IsValid()) {
    return transmissionPipeline_;
  }

  GraphicsPipelineDesc desc{};
  desc.vertexShader = staticMeshVS_;
  desc.fragmentShader = staticMeshFS_;
  desc.topology = PrimitiveTopology::TriangleList;
  desc.colorFormat = colorFormat_;
  desc.debugName = "SceneRenderer.TransmissionPipeline";

  desc.vertexLayouts = GetMeshVertexLayout();

  BindingLayoutHandle setLayouts[] = {
      GetTextureRegistry().GetBindingLayout(),
      frameLayout_,
      iblResources_.descriptorSetLayout,
      opaqueSceneLayout_,
      objectDataLayout_
  };

  desc.layout.descriptorSetLayouts = setLayouts;
  desc.layout.descriptorSetLayoutCount = 5;

  desc.raster.cullBackFaces = true;
  desc.raster.frontFaceCCW = true;
  desc.raster.wireframe = false;

  desc.depth.depthTestEnable = true;
  desc.depth.depthWriteEnable = true;
  desc.depth.depthFormat = depthFormat_;

  desc.blend.enable = false;

  transmissionPipeline_ = device_->CreateGraphicsPipeline(desc);
  return transmissionPipeline_;
}

PipelineHandle SceneRenderer::GetOrCreateTonemappingPipeline() {
  if (tonemappingPipeline_.IsValid()) {
    return tonemappingPipeline_;
  }

  GraphicsPipelineDesc desc{};
  desc.vertexShader = postProcessingVS_;
  desc.fragmentShader = tonemappingFS_;
  desc.topology = PrimitiveTopology::TriangleList;
  desc.colorFormat = colorFormat_;
  desc.debugName = "SceneRenderer.TonemappingPipeline";

  desc.vertexLayouts = {};

  BindingLayoutHandle setLayouts[] = {postProcessingLayout_};

  desc.layout.descriptorSetLayouts = setLayouts;
  desc.layout.descriptorSetLayoutCount = 1;

  desc.raster.cullBackFaces = false;
  desc.raster.frontFaceCCW = true;
  desc.raster.wireframe = false;

  desc.depth.depthTestEnable = false;
  desc.depth.depthWriteEnable = false;
  desc.depth.depthFormat = depthFormat_;

  desc.blend.enable = false;

  tonemappingPipeline_ = device_->CreateGraphicsPipeline(desc);
  return tonemappingPipeline_;
}

void SceneRenderer::UpdateOpaqueSceneDescriptor() {
  const auto &opaqueScene = graph_.GetImage("OpaqueScene");

  BindingImageInfo opaqueSceneImage{};
  opaqueSceneImage.imageView = opaqueScene.view;
  opaqueSceneImage.sampler = opaqueScene.sampler;
  opaqueSceneImage.imageLayout = ImageLayout::ShaderReadOnly;

  BindingImageInfo dummyImageInfo{};
  dummyImageInfo.imageView = opaqueSceneFallback_.dummy.view;
  dummyImageInfo.sampler = opaqueSceneFallback_.dummy.sampler;
  dummyImageInfo.imageLayout = ImageLayout::ShaderReadOnly;

  device_->UpdateBindingSet({
      .dstSet = opaqueSceneSet_,
      .binding = 0,
      .type = BindingType::CombinedImageSampler,
      .imageInfo = &opaqueSceneImage,
      .descriptorCount = 1,
  });

  device_->UpdateBindingSet({
      .dstSet = dummmyOpaqueSceneSet_,
      .binding = 0,
      .type = BindingType::CombinedImageSampler,
      .imageInfo = &dummyImageInfo,
      .descriptorCount = 1,
  });
}

void SceneRenderer::UpdateFinalSceneDescriptor() {
  const auto &finalScene = graph_.GetImage("FinalSceneColor");

  BindingImageInfo finalSceneImage{};
  finalSceneImage.imageView = finalScene.view;
  finalSceneImage.sampler = finalScene.sampler;
  finalSceneImage.imageLayout = ImageLayout::ShaderReadOnly;

  device_->UpdateBindingSet({
      .dstSet = postProcessingSet_,
      .binding = 0,
      .type = BindingType::CombinedImageSampler,
      .imageInfo = &finalSceneImage,
      .descriptorCount = 1,
  });
}

void SceneRenderer::RenderShadowMaps(ICommandList &cmd,
                                     const RenderWorld &world, const Camera& camera) {
  const auto &lights = world.GetDirectionalLights();

  directionalShadow_.enabled = false;

  if (lights.empty()) {
    return;
  }

  const DirectionalLight &light = lights[0];

  directionalShadow_.direction = light.direction;
  directionalShadow_.color = light.color;
  directionalShadow_.intensity = light.intensity;

  if (!light.castsShadow) {
    return;
  }

  directionalShadow_.enabled = true;

  glm::vec3 lightDir = glm::normalize(light.direction);

  glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f);

  if (std::abs(glm::dot(lightDir, up)) > 0.99f) {
    up = glm::vec3(0.0f, 0.0f, 1.0f);
  }

  for (uint32_t i = 0; i < directionalShadow_.cascadeCount; i++) {
      glm::vec3 frustumCorners[8] = {
          { -1.0f, 1.0f, 0.0f },
          { 1.0f, 1.0f, 0.0f },
          { 1.0f, -1.0f, 0.0f },
          { -1.0f, -1.0f, 0.0f },
          { -1.0f, 1.0f, 1.0f },
          { 1.0f, 1.0f, 1.0f },
          { 1.0f, -1.0f, 1.0f },
          { -1.0f, -1.0f, 1.0f },
      };

      glm::mat4 cameraView = camera.GetView();
      glm::mat4 cameraViewProj =
          camera.GetProjectionFromCascade(i) * cameraView;

      glm::mat4 invViewProj = glm::inverse(cameraViewProj);

      for (int j = 0; j < 8; j++) {
        glm::vec4 corner = invViewProj * glm::vec4(frustumCorners[j], 1.0f);
        frustumCorners[j] = glm::vec3(corner) / corner.w;
      }

      glm::vec3 frustumCenter = glm::vec3{ 0.0, 0.0, 0.0 };
      for (int j = 0; j < 8; j++) {
          frustumCenter += frustumCorners[j];
      }
      frustumCenter /= 8.0f;

      float radius = 0.0f;

      for (const glm::vec3& corner : frustumCorners) {
          radius = glm::max(
              radius,
              glm::length(corner - frustumCenter)
          );
      }

      // Quantizing the bounding-sphere radius keeps the orthographic
      // projection scale fixed while the camera rotates. This is the other
      // half of stable CSMs; texel snapping alone only stabilizes translation.
      constexpr float kCascadeRadiusQuantization = 16.0f;
      radius = std::ceil(radius * kCascadeRadiusQuantization) /
               kCascadeRadiusQuantization;

      const glm::vec3 lightPosition =
          frustumCenter - lightDir * radius;

      const glm::mat4 lightView =
          glm::lookAtRH(lightPosition, frustumCenter, up);

      glm::mat4 lightProjection = glm::orthoRH_ZO(
          -radius,
          radius,
          -radius,
          radius,
          -radius * 6.0f,
          radius * 6.0f
      );

      // Keep the light projection aligned to shadow-map texels. Without this,
      // tiny camera movements continuously move world geometry across texels
      // and make otherwise static shadow edges shimmer.
      glm::vec4 shadowOrigin =
          lightProjection * lightView * glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
      shadowOrigin *=
          static_cast<float>(directionalShadow_.resolution) * 0.5f;

      const glm::vec4 roundedOrigin = glm::round(shadowOrigin);
      glm::vec4 roundingOffset = roundedOrigin - shadowOrigin;
      roundingOffset *=
          2.0f / static_cast<float>(directionalShadow_.resolution);

      lightProjection[3][0] += roundingOffset.x;
      lightProjection[3][1] += roundingOffset.y;

      glm::mat4 lightViewProj = lightProjection * lightView;

      directionalShadow_.lightViewProjs[i] = lightViewProj;

      DepthAttachmentDesc dDesc = {
          .view = directionalShadow_.views[i],
          .loadOp = LoadOp::Clear,
          .storeOp = StoreOp::Store,
          .clearDepth = 1.0f,
      };

      const uint32_t resolution = directionalShadow_.resolution;

      cmd.BeginRendering({
          .renderArea = {.offset = {0, 0}, .extent = {resolution, resolution}},
          .colorAttachments = nullptr,
          .colorAttachmentCount = 0,
          .depthAttachment = &dDesc,
          });

      cmd.SetViewport({
          .x = 0.0f,
          .y = 0.0f,
          .width = static_cast<float>(resolution),
          .height = static_cast<float>(resolution),
          .minDepth = 0.0f,
          .maxDepth = 1.0f,
          });

      cmd.SetScissor({
          .offset = {0, 0},
          .extent = {resolution, resolution},
          });

      PipelineHandle pipeline = GetOrCreateShadowPipeline();
      cmd.BindPipeline(pipeline);

      const auto drawShadowCaster = [&](const StaticMeshRenderItem& item) {
          if (!item.mesh) {
              return;
          }

          ShadowPushConstants pc{};
          pc.model = item.worldTransform.ToMatrix() * item.localTransform.ToMatrix();
          pc.lightViewProj = lightViewProj;

          cmd.PushConstants(ShaderStage::Vertex, 0, sizeof(ShadowPushConstants), &pc);

          meshRenderer_.DrawDepthOnly(&cmd, *item.mesh, *item.submesh);
      };

      for (const StaticMeshRenderItem& item : opaques_) {
          drawShadowCaster(item);
      }

      // Transmission is a surface-lighting property, not an instruction to
      // remove the object from the shadow caster set. Until colored transparent
      // shadows are supported, render it as an opaque caster.
      for (const StaticMeshRenderItem& item : transmissions_) {
          drawShadowCaster(item);
      }

      cmd.EndRendering();
  }
}

void SceneRenderer::BuildStaticMeshRenderList(const RenderWorld &world) {
  opaques_.clear();
  transmissions_.clear();
  alphaBlends_.clear();

  BuildGpuSceneData(world);
  gpuDrawsCpu_.clear();
  gpuBatches_.clear();

  const auto& objects = world.GetObjects();
  for (size_t objectIndex = 0; objectIndex < objects.size(); ++objectIndex) {
    const RenderObject& object = objects[objectIndex];
    if (!object.visible) {
      continue;
    }

    const MeshResource &mesh = world.GetMesh(object.mesh);

    for (const Submesh &submesh : mesh.submeshes) {
      StaticMeshRenderItem item{};
      item.mesh = &mesh;
      item.worldTransform = object.worldTransform;
      item.localTransform = object.localTransform;
      item.objectId = static_cast<uint32_t>(objectIndex);
      item.drawDataIndex = static_cast<uint32_t>(gpuDrawsCpu_.size());

      const MaterialResource *material = nullptr;

      MaterialHandle handle{};

      if (submesh.materialSlot >= 0 &&
          submesh.materialSlot < static_cast<int>(object.materials.size())) {
        handle = object.materials[submesh.materialSlot];

        if (handle.IsValid()) {
          material = &world.GetMaterial(handle);
        }
      }
      item.materialHandle = handle;
      item.material = material;
      item.submesh = &submesh;

      GPUDrawData drawData{
        .objectIndex = item.objectId,
        .materialIndex = materialGpuIndex_[item.materialHandle.id],
        .flags = submesh.hasTangents ? GPUDrawFlag_HasTangents : 0u,
      };

      gpuDrawsCpu_.push_back(drawData);

      bool isTransmission =
          material && material->transmission.transmissionFactor > 0.001f;

      bool isBlend = material && material->alphaMode == AlphaMode::Blend;

      if (isTransmission) {
        transmissions_.push_back(item);
      } else if (isBlend) {
        alphaBlends_.push_back(item);
      } else {
        opaques_.push_back(item);
        BuildOpaqueGpuBatch(item);
      }
    }
  }

  UploadGpuData();
}

void SceneRenderer::RenderOpaqueMeshes(ICommandList &cmd, const Camera &camera,
                                       DebugContext dbgCtx,
                                       BindingSetHandle set) {
  RenderStaticMeshesIndirect(cmd, set);
}

void SceneRenderer::RenderStaticMeshesIndirect(ICommandList &cmd,
                                                BindingSetHandle sceneSet) {
  if (!indirectCommands_ || indirectCommandsCpu_.empty()) {
    return;
  }

  PipelineHandle lastPipeline{};
  for (const auto &[key, batch] : gpuBatches_) {
    if (batch.indirectCommandCount == 0) {
      continue;
    }

    const PipelineHandle pipeline = GetOrCreatePipeline(key.pipeline);
    if (pipeline.id != lastPipeline.id) {
      cmd.BindPipeline(pipeline);
      cmd.SetBindings(pipeline, 0, GetTextureRegistry().GetBindingSet());
      cmd.SetBindings(pipeline, 1, frameSet_);
      if (iblReady_) {
        cmd.SetBindings(pipeline, 2, iblResources_.descriptorSet);
      }
      cmd.SetBindings(pipeline, 3, sceneSet);
      cmd.SetBindings(pipeline, 4, objectDataSet_);
      lastPipeline = pipeline;
    }

    cmd.BindVertexBuffer(0, key.vertexBuffer, 0);
    cmd.BindIndexBuffer(key.indexBuffer, key.indexType, 0);

    const uint64_t byteOffset =
        static_cast<uint64_t>(batch.indirectCommandOffset) *
        sizeof(Velos::RHI::DrawIndexedIndirectCommand);
    cmd.DrawIndexedIndirect(indirectCommands_, byteOffset,
                            batch.indirectCommandCount,
                            sizeof(Velos::RHI::DrawIndexedIndirectCommand));
  }
}

void SceneRenderer::RenderTransmissionMeshes(ICommandList &cmd,
                                             const Camera &camera,
                                             DebugContext dbgCtx) {
  if (transmissions_.empty()) {
    return;
  }

  PipelineHandle pipeline = GetOrCreateTransmissionPipeline();

  cmd.BindPipeline(pipeline);
  cmd.SetBindings(pipeline, 0, GetTextureRegistry().GetBindingSet());
  cmd.SetBindings(pipeline, 1, frameSet_);

  if (iblReady_) {
    cmd.SetBindings(pipeline, 2, iblResources_.descriptorSet);
  }

  cmd.SetBindings(pipeline, 3, opaqueSceneSet_);
  cmd.SetBindings(pipeline, 4, objectDataSet_);

  BufferHandle lastVB{};
  BufferHandle lastIB{};

  for (const StaticMeshRenderItem &item : transmissions_) {
    if (!item.mesh || !item.submesh) {
      continue;
    }

    const Submesh &submesh = *item.submesh;
    const MaterialResource *material = item.material;

    if (item.mesh->vertexBuffer.id != lastVB.id) {
      cmd.BindVertexBuffer(0, item.mesh->vertexBuffer, 0);
      lastVB = item.mesh->vertexBuffer;
    }

    if (item.mesh->indexBuffer.id != lastIB.id) {
      cmd.BindIndexBuffer(item.mesh->indexBuffer, IndexType::U32, 0);
      lastIB = item.mesh->indexBuffer;
    }

    meshRenderer_.DrawSubmeshBound(&cmd, submesh, item.drawDataIndex);
  }
}
void SceneRenderer::RenderAlphaBlendMeshes(ICommandList &cmd,
                                           const Camera &camera,
                                           DebugContext dbgCtx) {
  RenderStaticMeshesDirect(cmd, camera, dbgCtx, alphaBlends_, opaqueSceneSet_);
}

void SceneRenderer::RenderStaticMeshesDirect(
    ICommandList &cmd, const Camera &camera, DebugContext dbgCtx,
    const std::vector<StaticMeshRenderItem> &items,
    BindingSetHandle sceneSet) {

  PipelineHandle lastPipeline{};
  BindingSetHandle lastMaterialSet{};
  BindingSetHandle lastSceneSet{};
  BufferHandle lastVertexBuffer{};
  BufferHandle lastIndexBuffer{};

  for (const StaticMeshRenderItem &item : items) {
    if (!item.mesh || !item.submesh) {
      continue;
    }

    const MeshResource &mesh = *item.mesh;
    const Submesh &submesh = *item.submesh;
    const MaterialResource *material = item.material;

    MeshPipelineKey key{};
    if (material) {
      key.alphaMode = material->alphaMode;
      key.doubleSided = material->doubleSided;
    }

    PipelineHandle pipeline = GetOrCreatePipeline(key);

    if (pipeline.id != lastPipeline.id) {
      cmd.BindPipeline(pipeline);
      cmd.SetBindings(pipeline, 0, GetTextureRegistry().GetBindingSet());
      cmd.SetBindings(pipeline, 1, frameSet_);

      if (iblReady_) {
        cmd.SetBindings(pipeline, 2, iblResources_.descriptorSet);
      }
      cmd.SetBindings(pipeline, 4, objectDataSet_);

      lastPipeline = pipeline;
      lastMaterialSet = {};
      lastSceneSet = {};
      lastVertexBuffer = {};
      lastIndexBuffer = {};
    }

    if (sceneSet.id != lastSceneSet.id) {
      cmd.SetBindings(pipeline, 3, sceneSet);
      lastSceneSet = sceneSet;
    }

    if (mesh.vertexBuffer.id != lastVertexBuffer.id) {
      cmd.BindVertexBuffer(0, mesh.vertexBuffer, 0);
      lastVertexBuffer = mesh.vertexBuffer;
    }

    if (mesh.indexBuffer.id != lastIndexBuffer.id) {
      cmd.BindIndexBuffer(mesh.indexBuffer, IndexType::U32, 0);
      lastIndexBuffer = mesh.indexBuffer;
    }

    meshRenderer_.DrawSubmeshBound(&cmd, submesh, item.drawDataIndex);
  }
}
void SceneRenderer::RenderPostProcessingEffect(ICommandList &cmd,
                                               const FrameRenderContext &frame,
                                               const DebugContext &dbgCtx) {
  PipelineHandle postProcessingPipeline = GetOrCreateTonemappingPipeline();

  cmd.BindPipeline(postProcessingPipeline);

  cmd.SetBindings(postProcessingPipeline, 0, postProcessingSet_);

  cmd.Draw(3);
}

void SceneRenderer::RenderDebug(ICommandList &cmd, const RenderWorld &world,
                                const Camera &camera, DebugContext dbgCtx) {
  if (!lineRenderer3D_) {
    return;
  }

  lineRenderer3D_->clear();

  const glm::vec4 objectBoundsColor{1.0f, 1.0f, 1.0f, 0.8f};
  const glm::vec4 submeshBoundsColor{1.0f, 1.0f, 0.0f, 0.8f};
  const glm::vec4 lightColor{1.0f, 1.0f, 0.0f, 1.0f};

  AABB sceneBounds;

  if (dbgCtx.drawSceneBounds || dbgCtx.drawMeshBounds ||
      dbgCtx.drawLightDirection) {

    for (const auto &mesh : opaques_) {
      const glm::mat4 model =
          mesh.worldTransform.ToMatrix() * mesh.localTransform.ToMatrix();

      AABB objectBounds = mesh.mesh->aabb.Transform(model);

      sceneBounds.Expand(objectBounds.lower);
      sceneBounds.Expand(objectBounds.upper);

      if (dbgCtx.drawSceneBounds) {
        lineRenderer3D_->aabb(objectBounds.lower, objectBounds.upper,
                              objectBoundsColor);
      }

      if (dbgCtx.drawMeshBounds) {
        for (const auto &submesh : mesh.mesh->submeshes) {
          AABB submeshBounds = submesh.aabb.Transform(model);
          lineRenderer3D_->aabb(submeshBounds.lower, submeshBounds.upper,
                                submeshBoundsColor);
        }
      }
    }
  }

  if (dbgCtx.drawLightDirection) {
    const auto &lights = world.GetDirectionalLights();

    if (!lights.empty()) {
      const DirectionalLight &light = lights[0];

      glm::vec3 center = sceneBounds.Center();
      glm::vec3 size = sceneBounds.upper - sceneBounds.lower;

      float sceneRadius = glm::length(size) * 0.5f;
      float arrowLength = sceneRadius * 0.35f;
      float headSize = arrowLength * 0.15f;

      glm::vec3 dir = glm::normalize(light.direction);

      glm::vec3 anchor =
          center + glm::vec3(0.0f, size.y * 0.5f + sceneRadius * 0.1f, 0.0f);

      glm::vec3 start = anchor - dir * arrowLength;
      glm::vec3 end = anchor;

      lineRenderer3D_->arrow(start, end, lightColor, headSize);
    }
  }

  lineRenderer3D_->render(cmd, camera.GetProjection() * camera.GetView());
  lineRenderer3D_->clear();
}

void SceneRenderer::RenderOpaquePass(ICommandList &cmd,
                                     const FrameRenderContext &frame,
                                     const Camera &camera,
                                     const DebugContext &dbgCtx) {
  BeginOpaquePass(cmd, frame, camera, dbgCtx);

  RenderOpaqueMeshes(cmd, camera, dbgCtx, dummmyOpaqueSceneSet_);

  if (environment_) {
    skyboxPass_.Render(cmd, *environment_, camera.GetView(),
                       camera.GetProjection());
  }

  EndOpaquePass(cmd);
}

void SceneRenderer::RenderMainPass(ICommandList &cmd, const RenderWorld &world,
                                   const FrameRenderContext &frame,
                                   const Camera &camera,
                                   const DebugContext &dbgCtx) {
  ZoneScopedN("SceneRenderer::RenderMainPass");

  {
    ZoneScopedN("BeginMainPass");
    BeginMainPass(cmd, frame, camera, dbgCtx);
  }

  {
    ZoneScopedN("RenderOpaqueMeshes");
    RenderOpaqueMeshes(cmd, camera, dbgCtx, opaqueSceneSet_);
  }

  {
    ZoneScopedN("Skybox");
    if (environment_) {
      skyboxPass_.Render(cmd, *environment_, camera.GetView(),
                         camera.GetProjection());
    }
  }

  {
    ZoneScopedN("RenderTransmissionMeshes");
    RenderTransmissionMeshes(cmd, camera, dbgCtx);
  }

  {
    ZoneScopedN("RenderAlphaBlendMeshes");
    RenderAlphaBlendMeshes(cmd, camera, dbgCtx);
  }

  {
    ZoneScopedN("RenderDebug");
    RenderDebug(cmd, world, camera, dbgCtx);
  }

  {
    ZoneScopedN("RenderUI");

    if (frame.renderUi) {
      frame.renderUi(cmd);
    }
  }

  {
    ZoneScopedN("EndMainPass");
    EndMainPass(cmd);
  }
}

void SceneRenderer::RenderTonemappingPass(ICommandList &cmd,
                                          const RenderWorld &world,
                                          const FrameRenderContext &frame,
                                          const Camera &camera,
                                          const DebugContext &dbgCtx) {
  BeginTonemappingPass(cmd, frame, camera, dbgCtx);

  RenderPostProcessingEffect(cmd, frame, dbgCtx);

  EndTonemappingPass(cmd);
}

void SceneRenderer::RenderUIPass(ICommandList &cmd,
                                 const FrameRenderContext &frame) {}

void SceneRenderer::BeginMainPass(ICommandList &cmd,
                                  const FrameRenderContext &frame,
                                  const Camera &camera,
                                  const DebugContext &dbgCtx) {
    FrameDataGPU frameData{};

    frameData.view = camera.GetView();
    frameData.proj = camera.GetProjection();

    const uint32_t cascadeCount = std::min(
        directionalShadow_.cascadeCount,
        k_MaxShadowCascades
    );
    frameData.cascadeCount = cascadeCount;

    for (uint32_t i = 0; i < cascadeCount; ++i) {
        frameData.cascades[i].lightViewProj =
            directionalShadow_.lightViewProjs[i];

        frameData.cascades[i].splitData = glm::vec4(
            camera.GetCascadeFarDistance(i),
            0.0f,
            0.0f,
            0.0f
        );
    }

    frameData.lightDirection =
        glm::vec4(directionalShadow_.direction, 0.0f);

    frameData.lightColor =
        glm::vec4(directionalShadow_.color, 1.0f);

    frameData.viewportSize = glm::vec2(
        static_cast<float>(frame.extent.width),
        static_cast<float>(frame.extent.height)
    );

    frameData.lightIntensity = directionalShadow_.intensity;
    frameData.shadowsEnabled = directionalShadow_.enabled ? 1 : 0;
    frameData.showMode = static_cast<int32_t>(dbgCtx.mode);

  cmd.UpdateBuffer(
      {.buffer = frameUBO_, .data = &frameData, .size = sizeof(FrameDataGPU)});

  const auto &finalColor = graph_.GetImage("FinalSceneColor");
  const auto &finalDepth = graph_.GetImage("FinalSceneDepth");

  ColorAttachmentDesc colorAttachment{};
  colorAttachment.view = finalColor.view;
  colorAttachment.loadOp = LoadOp::Clear;
  colorAttachment.storeOp = StoreOp::Store;
  colorAttachment.clearValue = {1.0f, 0.0f, 0.0f, 1.0f};

  DepthAttachmentDesc depthAttachment{};
  depthAttachment.view = finalDepth.view;
  depthAttachment.loadOp = LoadOp::Clear;
  depthAttachment.storeOp = StoreOp::Store;
  depthAttachment.clearDepth = 1.0f;
  depthAttachment.clearStencil = 0;

  Rect2D renderArea{};
  renderArea.offset = {0, 0};
  renderArea.extent = frame.extent;

  RenderingInfo renderingInfo{};
  renderingInfo.renderArea = renderArea;
  renderingInfo.colorAttachments = &colorAttachment;
  renderingInfo.colorAttachmentCount = 1;
  renderingInfo.depthAttachment = &depthAttachment;

  cmd.BeginRendering(renderingInfo);

  cmd.SetViewport({
      .x = 0.0f,
      .y = 0.0f,
      .width = static_cast<float>(frame.extent.width),
      .height = static_cast<float>(frame.extent.height),
      .minDepth = 0.0f,
      .maxDepth = 1.0f,
  });

  cmd.SetScissor({
      .offset = {0, 0},
      .extent = frame.extent,
  });
}

void SceneRenderer::BeginOpaquePass(ICommandList &cmd,
                                    const FrameRenderContext &frame,
                                    const Camera &camera,
                                    const DebugContext dbgCtx) {
    FrameDataGPU frameData{};

    frameData.view = camera.GetView();
    frameData.proj = camera.GetProjection();

    const uint32_t cascadeCount = std::min(
        directionalShadow_.cascadeCount,
        k_MaxShadowCascades
    );
    frameData.cascadeCount = cascadeCount;

    for (uint32_t i = 0; i < cascadeCount; ++i) {
        frameData.cascades[i].lightViewProj =
            directionalShadow_.lightViewProjs[i];

        frameData.cascades[i].splitData = glm::vec4(
            camera.GetCascadeFarDistance(i),
            0.0f,
            0.0f,
            0.0f
        );
    }

    frameData.lightDirection =
        glm::vec4(directionalShadow_.direction, 0.0f);

    frameData.lightColor =
        glm::vec4(directionalShadow_.color, 1.0f);

    frameData.viewportSize = glm::vec2(
        static_cast<float>(frame.extent.width),
        static_cast<float>(frame.extent.height)
    );

    frameData.lightIntensity = directionalShadow_.intensity;
    frameData.shadowsEnabled = directionalShadow_.enabled ? 1 : 0;
    frameData.showMode = static_cast<int32_t>(dbgCtx.mode);

  cmd.UpdateBuffer(
      {.buffer = frameUBO_, .data = &frameData, .size = sizeof(FrameDataGPU)});

  const auto &opaqueScene = graph_.GetImage("OpaqueScene");

  ColorAttachmentDesc colorAttachment{};
  colorAttachment.view = opaqueScene.renderView;
  colorAttachment.loadOp = LoadOp::Clear;
  colorAttachment.storeOp = StoreOp::Store;
  colorAttachment.clearValue = {0.1f, 0.1f, 0.1f, 1.0f};

  DepthAttachmentDesc depthAttachment{};
  depthAttachment.view = frame.depthView;
  depthAttachment.loadOp = LoadOp::Clear;
  depthAttachment.storeOp = StoreOp::Store;
  depthAttachment.clearDepth = 1.0f;
  depthAttachment.clearStencil = 0;

  Rect2D renderArea{};
  renderArea.offset = {0, 0};
  renderArea.extent = frame.extent;

  RenderingInfo renderingInfo{};
  renderingInfo.renderArea = renderArea;
  renderingInfo.colorAttachments = &colorAttachment;
  renderingInfo.colorAttachmentCount = 1;
  renderingInfo.depthAttachment = &depthAttachment;

  cmd.BeginRendering(renderingInfo);

  cmd.SetViewport({
      .x = 0.0f,
      .y = 0.0f,
      .width = static_cast<float>(frame.extent.width),
      .height = static_cast<float>(frame.extent.height),
      .minDepth = 0.0f,
      .maxDepth = 1.0f,
  });

  cmd.SetScissor({
      .offset = {0, 0},
      .extent = frame.extent,
  });
}

void SceneRenderer::BeginTonemappingPass(ICommandList &cmd,
                                         const FrameRenderContext &frame,
                                         const Camera &camera,
                                         const DebugContext &dbgCtx) {
  ColorAttachmentDesc colorAttachment{};
  colorAttachment.view = frame.backbufferView;
  colorAttachment.loadOp = LoadOp::Clear;
  colorAttachment.storeOp = StoreOp::Store;
  colorAttachment.clearValue = {0.1f, 0.1f, 0.1f, 1.0f};

  DepthAttachmentDesc depthAttachment{};
  depthAttachment.view = frame.depthView;
  depthAttachment.loadOp = LoadOp::Clear;
  depthAttachment.storeOp = StoreOp::Store;
  depthAttachment.clearDepth = 1.0f;
  depthAttachment.clearStencil = 0;

  Rect2D renderArea{};
  renderArea.offset = {0, 0};
  renderArea.extent = frame.extent;

  RenderingInfo renderingInfo{};
  renderingInfo.renderArea = renderArea;
  renderingInfo.colorAttachments = &colorAttachment;
  renderingInfo.colorAttachmentCount = 1;
  renderingInfo.depthAttachment = &depthAttachment;

  cmd.BeginRendering(renderingInfo);

  cmd.SetViewport({
      .x = 0.0f,
      .y = 0.0f,
      .width = static_cast<float>(frame.extent.width),
      .height = static_cast<float>(frame.extent.height),
      .minDepth = 0.0f,
      .maxDepth = 1.0f,
  });

  cmd.SetScissor({
      .offset = {0, 0},
      .extent = frame.extent,
  });
}

void SceneRenderer::EndMainPass(ICommandList &cmd) { cmd.EndRendering(); }

void SceneRenderer::EndOpaquePass(ICommandList &cmd) { cmd.EndRendering(); }

void SceneRenderer::EndTonemappingPass(ICommandList &cmd) {
  cmd.EndRendering();
}

void SceneRenderer::UploadMaterialBuffer(ICommandList &command,
                                         const RenderWorld &world) {
  materialGpuIndex_.clear();

  TextureRegistry& textureRegistry = GetTextureRegistry();

  std::vector<MaterialDataGPU> gpuMaterials;
  gpuMaterials.reserve(world.GetMaterials().size());

  uint32_t gpuIndex = 0;

  for (const auto &[handleId, material] : world.GetMaterials()) {
    materialGpuIndex_[handleId] = gpuIndex++;

    MaterialDataGPU gpu{};
    gpu.baseColorFactor = material.baseColorFactor;
    gpu.metallicFactor = material.metallicFactor;
    gpu.roughnessFactor = material.roughnessFactor;
    gpu.alphaCutoff = material.alphaCutoff;
    gpu.alphaMode = static_cast<int>(material.alphaMode);
    gpu.hasMaterial = 1;

    gpu.transmissionFactor = material.transmission.transmissionFactor;
    gpu.thicknessFactor = material.volume.thicknessFactor;
    gpu.ior = material.ior;
    gpu.clearcoatFactor = material.clearcoat.factor;
    gpu.clearcoatRoughnessFactor = material.clearcoat.roughnessFactor;
    gpu.attenuationColorDistance = glm::vec4(
        material.volume.attenuationColor, material.volume.attenuationDistance);
    gpu.emissiveFactor = material.emissive.factor;
    gpu.emissiveStrength = material.emissive.strength;
    gpu.useUnlit = material.useUnlit;

    gpu.baseColorTextureIndex =
        textureRegistry.GetTextureIndex(material.baseColorTextureHandle);

    gpu.baseColorTextureTransformation = material.baseColorTextureTransformation;

    gpu.normalTextureIndex = 
        textureRegistry.GetTextureIndex(material.normalTextureHandle);

    gpu.normalTextureTransformation = material.normalTextureTransformation;

    gpu.metallicRoughnessTextureIndex =
        textureRegistry.GetTextureIndex(material.metallicRoughnessTextureHandle);

    gpu.metallicRoughnessTextureTransformation = material.metallicRoughnessTextureTransformation;

    gpu.occlusionTextureIndex =
        textureRegistry.GetTextureIndex(material.occlusionTextureHandle);

    gpu.occlusionTextureTransformation = material.occlusionTextureTransformation;

    gpu.transmissionTextureIndex =
        textureRegistry.GetTextureIndex(material.transmission.transmissionTextureHandle);

    gpu.transmissionTextureTransformation = material.transmission.transmissionTextureTransformation;

    gpu.thicknessTextureIndex =
        textureRegistry.GetTextureIndex(
            material.volume.thicknessTextureHandle);

    gpu.thicknessTextureTransformation = material.volume.thicknessTextureTransformation;

    gpu.clearcoatTextureIndex =
        textureRegistry.GetTextureIndex(
            material.clearcoat.textureHandle);

    gpu.clearcoatTextureTransformation = material.clearcoat.textureTransformation;

    gpu.clearcoatRoughnessTextureIndex =
        textureRegistry.GetTextureIndex(
            material.clearcoat.roughnessTextureHandle);

    gpu.clearcoatRoughnessTextureTransformation = material.clearcoat.roughnessTextureTransformation;

    gpu.clearcoatNormalTextureIndex =
        textureRegistry.GetTextureIndex(
            material.clearcoat.normalTextureHandle);

    gpu.clearcoatNormalTextureTransformation = material.clearcoat.normalTextureTransformation;

    gpu.emissiveTextureIndex =
        textureRegistry.GetTextureIndex(
            material.emissive.textureHandle);

    gpu.emissiveTextureTransformation = material.emissive.textureTransformation;

    gpuMaterials.push_back(gpu);
  }

  if (!gpuMaterials.empty()) {
    command.UpdateBuffer({
        .buffer = materialBuffer_,
        .offset = 0,
        .data = gpuMaterials.data(),
        .size = gpuMaterials.size() * sizeof(MaterialDataGPU),
    });
  }
}

void SceneRenderer::BuildGpuSceneData(const RenderWorld& world)
{
    const std::vector<RenderObject>& objects = world.GetObjects();

    objectToSlot_.clear();
    objectToSlot_.reserve(objects.size());
    freeSlots_.clear();

    if (slots_.size() > objects.size()) {
        slots_.resize(objects.size());
    }
    if (slots_.size() < objects.size()) {
        const size_t previousSize = slots_.size();
        slots_.resize(objects.size());
        for (size_t index = previousSize; index < objects.size(); ++index) {
            slots_[index] = GpuObjectSlot{
                .objectId = static_cast<uint32_t>(index),
                .generation = 1,
                .occupied = true,
            };
        }
    }

    for (size_t objectIndex = 0; objectIndex < objects.size(); ++objectIndex) {
        const uint32_t slot = static_cast<uint32_t>(objectIndex);
        const RenderObject& object = objects[objectIndex];

        GpuObjectSlot& slotInfo = slots_[slot];
        slotInfo.objectId = slot;
        slotInfo.occupied = true;
        objectToSlot_.emplace(slot, slot);

        const MeshResource& mesh = world.GetMesh(object.mesh);
        UploadIfChanged(slot, ConvertToGpuObject(object, mesh));
    }
}

void SceneRenderer::BuildOpaqueGpuBatch(const StaticMeshRenderItem& item)
{
    MeshPipelineKey pipelineKey{};
    if (item.material) {
        pipelineKey.alphaMode = item.material->alphaMode;
        pipelineKey.doubleSided = item.material->doubleSided;
    }

    GpuBatchKey batchkey{
        .pipeline = pipelineKey,
        .vertexBuffer = item.mesh->vertexBuffer,
        .indexBuffer = item.mesh->indexBuffer,
        .indexType = IndexType::U32,
    };

    GpuBatch& batch = FindOrCreateBatch(batchkey);

    batch.draws.push_back(
        GpuBatchDraw{
            .drawDataIndex = item.drawDataIndex,
            .indexCount = item.submesh->indexCount,
            .firstIndex = item.submesh->firstIndex,
            .vertexOffset = 0,
        });
}

GpuBatch& SceneRenderer::FindOrCreateBatch(const GpuBatchKey& batchKey)
{
    const auto existing = gpuBatches_.find(batchKey);
    if (existing != gpuBatches_.end()) {
        return existing->second;
    }

    GpuBatch batch{};
    batch.key = batchKey;

    gpuBatches_.emplace(batchKey, batch);
    return gpuBatches_[batchKey];
}

GPUSceneObject SceneRenderer::ConvertToGpuObject(const RenderObject &object,
                                  const MeshResource &mesh) const
{
    GPUSceneObject out{};

    out.model =
        object.worldTransform.ToMatrix() * object.localTransform.ToMatrix();
    out.normalMatrix = glm::transpose(glm::inverse(out.model));

    const AABB worldBounds = mesh.aabb.Transform(out.model);
    const glm::vec3 center = worldBounds.Center();
    const glm::vec3 halfExtents = worldBounds.Extents() * 0.5f;

    out.boundingSphere = glm::vec4(center, glm::length(halfExtents));
    out.boundsMinimum = glm::vec4(worldBounds.lower, 0.0f);
    out.boundsMaximum = glm::vec4(worldBounds.upper, 0.0f);

    // Batch and material are assigned later, once this object has been
    // expanded into its submesh render items.
    out.drawData = glm::uvec4{
        0u,
        0u,
        object.objectId,
        object.visible ? 1u : 0u,
    };

    return out;
}

void SceneRenderer::UploadIfChanged(uint32_t slot,
                                    const GPUSceneObject& gpuObject)
{
    if (slot >= gpuObjectsCpu_.size()) {
        gpuObjectsCpu_.resize(slot + 1);
        gpuObjectsCpu_[slot] = gpuObject;
        dirtyGpuObjectSlots_.push_back(slot);
        return;
    }

    if (std::memcmp(&gpuObjectsCpu_[slot], &gpuObject,
                    sizeof(GPUSceneObject)) == 0) {
        return;
    }

    gpuObjectsCpu_[slot] = gpuObject;
    dirtyGpuObjectSlots_.push_back(slot);
}

void SceneRenderer::UploadGpuData()
{
    if (gpuObjectsCpu_.empty() || gpuDrawsCpu_.empty()) {
        return;
    }

    assert(gpuObjectsCpu_.size() <= k_MaxGpuObjects);
    assert(gpuDrawsCpu_.size() <= k_MaxGpuDraws);

    indirectCommandsCpu_.clear();
    indirectCommandsCpu_.reserve(gpuDrawsCpu_.size());
    for (auto& [key, batch] : gpuBatches_) {
        batch.indirectCommandOffset =
            static_cast<uint32_t>(indirectCommandsCpu_.size());
        batch.indirectCommandCount = static_cast<uint32_t>(batch.draws.size());

        for (const GpuBatchDraw& draw : batch.draws) {
            indirectCommandsCpu_.push_back(Velos::RHI::DrawIndexedIndirectCommand{
                .indexCount = draw.indexCount,
                .instanceCount = 1,
                .firstIndex = draw.firstIndex,
                .vertexOffset = draw.vertexOffset,
                .firstInstance = draw.drawDataIndex,
            });
        }
    }

    const uint64_t objectUploadSize =
        gpuObjectsCpu_.size() * sizeof(GPUSceneObject);
    const uint64_t drawUploadSize = gpuDrawsCpu_.size() * sizeof(GPUDrawData);
    const uint64_t indirectUploadSize = indirectCommandsCpu_.size() *
        sizeof(Velos::RHI::DrawIndexedIndirectCommand);

    auto context = device_->CreateUploadContext(
        objectUploadSize + drawUploadSize + indirectUploadSize);
    context->Begin();

    context->UploadBuffer({
        .dstBuffer = gpuObjectBuffer_,
        .dstOffset = 0,
        .size = objectUploadSize,
        .data = gpuObjectsCpu_.data(),
        });

    context->UploadBuffer({
        .dstBuffer = gpuDrawBuffer_,
        .dstOffset = 0,
        .size = drawUploadSize,
        .data = gpuDrawsCpu_.data(),
    });

    if (indirectUploadSize > 0) {
        context->UploadBuffer({
            .dstBuffer = indirectCommands_,
            .dstOffset = 0,
            .size = indirectUploadSize,
            .data = indirectCommandsCpu_.data(),
        });
    }

    context->Flush();
    dirtyGpuObjectSlots_.clear();


    BindingBufferInfo objectDataBuffer{};
    objectDataBuffer.buffer = gpuObjectBuffer_;
    objectDataBuffer.offset = 0;
    objectDataBuffer.range = sizeof(GPUSceneObject) * gpuObjectsCpu_.size();

    device_->UpdateBindingSet({
        .dstSet = objectDataSet_,
        .binding = 0,
        .type = BindingType::StorageBuffer,
        .bufferInfo = &objectDataBuffer,
        .descriptorCount = 1,
        });

    BindingBufferInfo drawDataBuffer{};
    drawDataBuffer.buffer = gpuDrawBuffer_;
    drawDataBuffer.offset = 0;
    drawDataBuffer.range = drawUploadSize;

    device_->UpdateBindingSet({
        .dstSet = objectDataSet_,
        .binding = 1,
        .type = BindingType::StorageBuffer,
        .bufferInfo = &drawDataBuffer,
        .descriptorCount = 1,
    });
}

} // namespace Rodan
