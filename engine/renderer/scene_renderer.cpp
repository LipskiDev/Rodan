#include "glm/ext/matrix_clip_space.hpp"
#include "glm/ext/matrix_transform.hpp"
#include "graphics/environment/environment_map.h"
#include "graphics/environment/ibl_baker.h"
#include "graphics/material_types.h"
#include "graphics/mesh_resource.h"
#include "graphics/shaders_types.h"
#include "graphics/texture.h"
#include "renderer/render_graph/render_graph_builder.h"
#include "rhi/rhi_command_list.h"
#include "rhi/rhi_handles.h"
#include "rhi/rhi_pipeline.h"
#include "rhi/rhi_resources.h"
#include "rhi/rhi_types.h"
#include "rhi/vulkan/vk_device.h"
#include "scene/handles.h"
#include "scene/render_world.h"
#include "tracy/Tracy.hpp"
#include <core/path.h>
#include <iostream>
#include <renderer/scene_renderer.h>
#include <stdexcept>
#include <string>

namespace Rodan {
void SceneRenderer::Initialize(IDevice *device, SwapchainHandle swapchain,
                               Format colorFormat, Format depthFormat,
                               DescriptorSetLayoutHandle materialLayout) {

  device_ = device;
  swapchain_ = swapchain;
  colorFormat_ = colorFormat;
  depthFormat_ = depthFormat;
  materialLayout_ = materialLayout;

  graphRenderer_ = std::make_unique<Debug::GraphRenderer>("Frame Graph");
  lineRenderer3D_ = std::make_unique<Debug::LineRenderer3D>(device);
  lineRenderer2D_ = std::make_unique<Debug::LineRenderer2D>();

  auto vertSpv = Velos::ShaderCompiler::CompileFile({
      .path = Velos::Path::Resolve("assets/shaders/static_mesh.vert").string(),
      .stage = ShaderStage::Vertex,
      .entryPoint = "main",
  });

  auto fragSpv = Velos::ShaderCompiler::CompileFile({
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

  auto depthVertSpv = Velos::ShaderCompiler::CompileFile({
      .path = Velos::Path::Resolve("assets/shaders/shadow_depth.vert").string(),
      .stage = ShaderStage::Vertex,
      .entryPoint = "main",
  });

  auto depthFragSpv = Velos::ShaderCompiler::CompileFile({
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

  vertSpv = Velos::ShaderCompiler::CompileFile({
      .path =
          Velos::Path::Resolve("assets/shaders/postprocessing.vert").string(),
      .stage = ShaderStage::Vertex,
      .entryPoint = "main",
  });

  fragSpv = Velos::ShaderCompiler::CompileFile({
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

  directionalShadow_.texture.image = device->CreateImage({
      .width = directionalShadow_.resolution,
      .height = directionalShadow_.resolution,
      .format = Format::D32_FLOAT,
      .usage = ImageUsage::DepthStencil | ImageUsage::Sampled,
      .debugName = "Directional Shadow Map",
  });

  directionalShadow_.texture.view = device->CreateImageView({
      .image = directionalShadow_.texture.image,
      .format = Format::D32_FLOAT,
      .aspect = ImageAspect::Depth,
  });

  directionalShadow_.texture.sampler = device->CreateSampler({
      .minFilter = Filter::Linear,
      .magFilter = Filter::Linear,
      .addressU = SamplerAddressMode::ClampToEdge,
      .addressV = SamplerAddressMode::ClampToEdge,
      .addressW = SamplerAddressMode::ClampToEdge,
  });

  directionalShadow_.enabled = true;

  DescriptorBindingDesc frameBindings[] = {
      {
          .binding = 0,
          .type = DescriptorType::CombinedImageSampler,
          .count = 1,
          .visibility = ShaderStage::Fragment,
      },
      {.binding = 1,
       .type = DescriptorType::UniformBuffer,
       .count = 1,
       .visibility = ShaderStage::Vertex | ShaderStage::Fragment},
      {.binding = 2,
       .type = DescriptorType::StorageBuffer,
       .count = 1,
       .visibility = ShaderStage::Fragment},
  };

  DescriptorBindingDesc environmentBindings[] = {
      {
          .binding = 0,
          .type = DescriptorType::CombinedImageSampler,
          .count = 1,
          .visibility = ShaderStage::Fragment,
      },
  };

  DescriptorPoolSize poolSizes[] = {
      {
          .type = DescriptorType::CombinedImageSampler,
          .count = 1,
      },
      {
          .type = DescriptorType::UniformBuffer,
          .count = 1,
      },
      {
          .type = DescriptorType::StorageBuffer,
          .count = 1,
      }};

  frameDescriptorPool_ = device_->CreateDescriptorPool({
      .poolSizes = poolSizes,
      .poolSizeCount = 3,
      .maxSets = 1,
  });

  frameLayout_ = device_->CreateDescriptorSetLayout({
      .bindings = frameBindings,
      .bindingCount = 3,
      .debugName = "SceneRenderer Frame Descriptor Set Layout",
  });

  DescriptorBindingDesc postProcessingBindings[] = {
      {
          .binding = 0,
          .type = DescriptorType::CombinedImageSampler,
          .count = 1,
          .visibility = ShaderStage::Fragment,
      },
  };

  postProcessingLayout_ = device_->CreateDescriptorSetLayout({
      .bindings = postProcessingBindings,
      .bindingCount = 1,
      .debugName = "SceneRenderer Post Processing Descriptor Set Layout",
  });

  DescriptorPoolSize poolSizesPostProcessing[] = {
      {
          .type = DescriptorType::CombinedImageSampler,
          .count = 1,
      },
  };

  postProcessingDescriptorPool_ = device_->CreateDescriptorPool({
      .poolSizes = poolSizesPostProcessing,
      .poolSizeCount = 1,
      .maxSets = 1,
  });

  postProcessingSet_ = device_->AllocateDescriptorSet(
      postProcessingDescriptorPool_, postProcessingLayout_);

  frameSet_ =
      device_->AllocateDescriptorSet(frameDescriptorPool_, frameLayout_);

  frameUBO_ = device_->CreateBuffer({.size = sizeof(FrameDataGPU),
                                     .usage = BufferUsage::Uniform,
                                     .memoryUsage = MemoryUsage::CPUToGPU,
                                     .debugName = "Scene Frame UBO"});

  DescriptorImageInfo finalImageInfo{};
  finalImageInfo.imageView = finalScene_.colorTexture.view;
  finalImageInfo.sampler = finalScene_.colorTexture.sampler;
  finalImageInfo.imageLayout = ImageLayout::ShaderReadOnly;

  materialBuffer_ = device_->CreateBuffer({
      .size = sizeof(MaterialDataGPU) * k_MaxMaterials,
      .usage = BufferUsage::Storage,
      .memoryUsage = MemoryUsage::CPUToGPU,
      .debugName = "Scene Material Buffer",
  });

  DescriptorBindingDesc opaqueSceneBindings[] = {
      {
          .binding = 0,
          .type = DescriptorType::CombinedImageSampler,
          .count = 1,
          .visibility = ShaderStage::Fragment,
      },
  };

  opaqueSceneLayout_ = device_->CreateDescriptorSetLayout({
      .bindings = opaqueSceneBindings,
      .bindingCount = 1,
      .debugName = "Opaque Scene Descriptor Set Layout",
  });

  DescriptorPoolSize opaqueScenePoolSizes[] = {
      {
          .type = DescriptorType::CombinedImageSampler,
          .count = 2,
      },
  };

  opaqueScenePool_ = device_->CreateDescriptorPool({
      .poolSizes = opaqueScenePoolSizes,
      .poolSizeCount = 1,
      .maxSets = 2,
  });

  opaqueSceneSet_ =
      device_->AllocateDescriptorSet(opaqueScenePool_, opaqueSceneLayout_);

  dummmyOpaqueSceneSet_ =
      device_->AllocateDescriptorSet(opaqueScenePool_, opaqueSceneLayout_);

  DescriptorImageInfo shadowImage{};
  shadowImage.imageView = directionalShadow_.texture.view;
  shadowImage.sampler = directionalShadow_.texture.sampler;
  shadowImage.imageLayout = ImageLayout::ShaderReadOnly;

  device->UpdateDescriptorSet({
      .dstSet = frameSet_,
      .binding = 0,
      .arrayElement = 0,
      .type = DescriptorType::CombinedImageSampler,
      .bufferInfo = nullptr,
      .imageInfo = &shadowImage,
      .descriptorCount = 1,
  });

  DescriptorBufferInfo frameBuffer{};
  frameBuffer.buffer = frameUBO_;
  frameBuffer.offset = 0;
  frameBuffer.range = sizeof(FrameDataGPU);

  device->UpdateDescriptorSet({
      .dstSet = frameSet_,
      .binding = 1,
      .type = DescriptorType::UniformBuffer,
      .bufferInfo = &frameBuffer,
      .descriptorCount = 1,
  });

  DescriptorBufferInfo materialBuffer{};
  materialBuffer.buffer = materialBuffer_;
  materialBuffer.offset = 0;
  materialBuffer.range = sizeof(MaterialDataGPU) * k_MaxMaterials;

  device_->UpdateDescriptorSet({
      .dstSet = frameSet_,
      .binding = 2,
      .type = DescriptorType::StorageBuffer,
      .bufferInfo = &materialBuffer,
      .descriptorCount = 1,
  });

  environment_ = EnvironmentMap::LoadHDR(
      device, Velos::Path::Resolve("assets/hdr/piazza_bologni_4k.hdr"));

  skyboxPass_.Initialize(device, colorFormat,
                         environment_->GetDescriptorSetLayout());

  iblBaker_ = std::make_unique<IBLBaker>();
  iblBaker_->Initialize(device, environment_->GetDescriptorSetLayout());
}

void SceneRenderer::Shutdown(IDevice *device) {
  for (auto &[key, pipeline] : pipelines_) {
    device->DestroyPipeline(pipeline);
  }
  pipelines_.clear();

  if (opaqueScene_.renderView.IsValid()) {
    device->DestroyImageView(opaqueScene_.renderView);
    opaqueScene_.renderView = {};
  }
  DestroyTexture(device, opaqueScene_.texture);
  DestroyTexture(device, opaqueScene_.dummy);
  opaqueScene_ = {};

  // Tonemapping / final scene target
  DestroyTexture(device, finalScene_.colorTexture);
  DestroyTexture(device, finalScene_.depthTexture);
  finalScene_ = {};

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

  if (postProcessingDescriptorPool_.IsValid()) {
    device->DestroyDescriptorPool(postProcessingDescriptorPool_);
    postProcessingDescriptorPool_ = {};
    postProcessingSet_ = {};
  }

  if (postProcessingLayout_.IsValid()) {
    device->DestroyDescriptorSetLayout(postProcessingLayout_);
    postProcessingLayout_ = {};
  }

  DestroyTexture(device, iblResources_.prefilterTexture);

  for (ImageViewHandle handle : iblResources_.prefilterFaceMipViews) {
    if (handle.IsValid()) {
      device->DestroyImageView(handle);
    }
  }

  DestroyTexture(device, iblResources_.irradianceTexture);
  device_->DestroyDescriptorSetLayout(iblResources_.descriptorSetLayout);
  device_->DestroyDescriptorPool(iblResources_.descriptorPool);
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
    device->DestroyDescriptorPool(opaqueScenePool_);
    opaqueScenePool_ = {};
    opaqueSceneSet_ = {};
  }

  if (opaqueSceneLayout_.IsValid()) {
    device->DestroyDescriptorSetLayout(opaqueSceneLayout_);
    opaqueSceneLayout_ = {};
  }

  device_->DestroySampler(directionalShadow_.texture.sampler);
  device_->DestroyImageView(directionalShadow_.texture.view);
  device_->DestroyImage(directionalShadow_.texture.image);

  device->DestroyBuffer(frameUBO_);
  device->DestroyBuffer(materialBuffer_);
  device->DestroyDescriptorSetLayout(frameLayout_);
  device->DestroyDescriptorPool(frameDescriptorPool_);

  graphRenderer_.reset();
  lineRenderer3D_.reset();
  lineRenderer2D_.reset();

  staticMeshes_.clear();

  device_ = nullptr;
  swapchain_ = {};
}

void SceneRenderer::Render(ICommandList &cmd, const RenderWorld &world,
                           const Camera &camera,
                           const FrameRenderContext &frame,
                           DebugContext dbgCtx) {
  EnsureOpaqueSceneTarget(frame);
  EnsureFinalSceneTarget(frame);
  graph_.Reset();
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
      "Build Static Mesh Render List", [](RenderGraphBuilder &) {},
      [&](ICommandList &) {
        BuildStaticMeshRenderList(world);

        tBuildList = now();
      });

  graph_.AddPass(
      "Upload Materials",
      [](RenderGraphBuilder &builder) {
        builder.StorageWrite("MaterialBuffer");
      },
      [&](ICommandList &cmd) {
        UploadMaterialBuffer(cmd, world);

        tUploadMaterials = now();
      });

  graph_.AddPass(
      "Shadow Maps",
      [](RenderGraphBuilder &builder) {
        builder.WriteDepthAttachment("DirectionalShadowMap");
      },
      [&](ICommandList &cmd) {
        RenderShadowMaps(cmd, world);

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
      [](RenderGraphBuilder &builder) {
        builder.StorageRead("MaterialBuffer");
        builder.ReadTexture("DirectionalShadowMap");
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

  for (uint32_t i = 1; i < opaqueScene_.mipLevels; i++) {
    graph_.AddPass(
        "Generate Opaque Scene Mip" + std::to_string(i),
        [this, i](RenderGraphBuilder &builder) {
          builder.CopySrc("OpaqueScene", {.baseMip = i - 1, .mipCount = 1});
          builder.CopyDst("OpaqueScene", {.baseMip = i, .mipCount = 1});
        },
        [this, &frame, i](ICommandList &cmd) {
          if (!transmissions_.empty()) {
            cmd.BlitMip(opaqueScene_.texture.image, frame.extent.width,
                        frame.extent.height, i - 1, i, 1);
          }
        });
  }

  graph_.AddPass(
      "Main Pass",
      [this](RenderGraphBuilder &builder) {
        builder.StorageRead("MaterialBuffer");
        builder.ReadTexture("DirectionalShadowMap");
        builder.ReadTexture("IBLResources");
        builder.ReadTexture("OpaqueScene",
                            {.baseMip = 0, .mipCount = opaqueScene_.mipLevels});

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

  graph_.ImportImage("DirectionalShadowMap", directionalShadow_.texture.image,
                     ImageAspect::Depth);

  graph_.ImportImage("OpaqueScene", opaqueScene_.texture.image,
                     ImageAspect::Color, opaqueScene_.mipLevels, 1);

  graph_.ImportImage("FinalSceneColor", finalScene_.colorTexture.image,
                     ImageAspect::Color);

  graph_.ImportImage("FinalSceneDepth", finalScene_.depthTexture.image,
                     ImageAspect::Depth);

  const ResourceState backbufferState =
      frame.backbufferLayout == ImageLayout::Present ? ResourceState::Present
                                                     : ResourceState::Undefined;
  graph_.ImportImage("Backbuffer", frame.backbufferImage, ImageAspect::Color, 1,
                     1, frame.backbufferLayout, backbufferState, true);

  graph_.ImportImage("FrameDepth", frame.depthImage, ImageAspect::Depth);

  graph_.Compile();
  graph_.Execute(cmd);

  opaques_.clear();
  transmissions_.clear();
  alphaBlends_.clear();

  const auto tFrameEnd = now();
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
    device->DestroyDescriptorSetLayout(iblResources_.descriptorSetLayout);
  }

  if (iblResources_.descriptorPool.IsValid()) {
    device->DestroyDescriptorPool(iblResources_.descriptorPool);
  }

  iblResources_ = {};

  if (environment_) {
    environment_->Destroy();
    environment_.reset();
  }

  environment_ = EnvironmentMap::LoadHDR(device, Velos::Path::Resolve(path));
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
  DescriptorSetLayoutHandle setLayouts[] = {
      materialLayout_,
      frameLayout_,
      iblResources_.descriptorSetLayout,
      opaqueSceneLayout_,
  };

  desc.layout.descriptorSetLayouts = setLayouts;
  desc.layout.descriptorSetLayoutCount = 4;

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

  DescriptorSetLayoutHandle setLayouts[] = {
      materialLayout_,
      frameLayout_,
      iblResources_.descriptorSetLayout,
      opaqueSceneLayout_,
  };

  desc.layout.descriptorSetLayouts = setLayouts;
  desc.layout.descriptorSetLayoutCount = 4;

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

  DescriptorSetLayoutHandle setLayouts[] = {postProcessingLayout_};

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

void SceneRenderer::EnsureOpaqueSceneTarget(const FrameRenderContext &frame) {
  bool recreated = false;

  if (opaqueScene_.texture.image.IsValid() &&
      opaqueScene_.extent.width == frame.extent.width &&
      opaqueScene_.extent.height == frame.extent.height) {
    return;
  }

  if (opaqueScene_.texture.image.IsValid()) {
    if (opaqueScene_.renderView.IsValid()) {
      device_->DestroyImageView(opaqueScene_.renderView);
      opaqueScene_.renderView = {};
    }
    DestroyTexture(device_, opaqueScene_.texture);
    DestroyTexture(device_, opaqueScene_.dummy);
    opaqueScene_ = {};
  }

  recreated = true;

  opaqueScene_.extent = frame.extent;

  uint32_t mipLevels =
      1u + static_cast<uint32_t>(std::floor(
               std::log2(std::max(frame.extent.width, frame.extent.height))));

  opaqueScene_.texture.image = device_->CreateImage({
      .width = frame.extent.width,
      .height = frame.extent.height,
      .mipLevels = mipLevels,
      .format = colorFormat_,
      .usage = ImageUsage::ColorAttachment | ImageUsage::Sampled |
               ImageUsage::TransferSrc | ImageUsage::TransferDst,
      .debugName = "Opaque Scene Color",
  });
  opaqueScene_.mipLevels = mipLevels;

  opaqueScene_.texture.view = device_->CreateImageView({
      .image = opaqueScene_.texture.image,
      .format = colorFormat_,
      .aspect = ImageAspect::Color,
      .baseMipLevel = 0,
      .mipLevelCount = opaqueScene_.mipLevels,
  });

  opaqueScene_.renderView = device_->CreateImageView({
      .image = opaqueScene_.texture.image,
      .format = colorFormat_,
      .aspect = ImageAspect::Color,
      .baseMipLevel = 0,
      .mipLevelCount = 1,
      .debugName = "Opaque Scene Color Render View",
  });

  opaqueScene_.texture.sampler = device_->CreateSampler({
      .minFilter = Filter::Linear,
      .magFilter = Filter::Linear,
      .addressU = SamplerAddressMode::ClampToEdge,
      .addressV = SamplerAddressMode::ClampToEdge,
      .addressW = SamplerAddressMode::ClampToEdge,
      .minLod = 0.0f,
      .maxLod = static_cast<float>(mipLevels - 1),
  });

  auto upload = device_->CreateUploadContext(4 * 256 * 1024 * 1024);
  upload->Begin();

  const std::uint8_t whitePixel[4] = {255, 255, 255, 255};

  opaqueScene_.dummy =
      CreateTexture2D(device_, upload.get(),
                      TextureDesc{
                          .width = 1,
                          .height = 1,
                          .format = Format::RGBA8_UNORM,
                          .minFilter = Filter::Linear,
                          .magFilter = Filter::Linear,
                          .addressU = SamplerAddressMode::Repeat,
                          .addressV = SamplerAddressMode::Repeat,
                          .addressW = SamplerAddressMode::Repeat,
                          .debugName = "Dummy OpaqueScene Texture",
                      },
                      whitePixel, 4);

  upload->Flush();

  if (recreated) {
    UpdateOpaqueSceneDescriptor();
  }
}

void SceneRenderer::UpdateOpaqueSceneDescriptor() {
  DescriptorImageInfo opaqueSceneImage{};
  opaqueSceneImage.imageView = opaqueScene_.texture.view;
  opaqueSceneImage.sampler = opaqueScene_.texture.sampler;
  opaqueSceneImage.imageLayout = ImageLayout::ShaderReadOnly;

  device_->UpdateDescriptorSet({
      .dstSet = opaqueSceneSet_,
      .binding = 0,
      .type = DescriptorType::CombinedImageSampler,
      .imageInfo = &opaqueSceneImage,
      .descriptorCount = 1,
  });

  DescriptorImageInfo dummyImage{};
  dummyImage.imageView = opaqueScene_.dummy.view;
  dummyImage.sampler = opaqueScene_.dummy.sampler;
  dummyImage.imageLayout = ImageLayout::ShaderReadOnly;

  device_->UpdateDescriptorSet({
      .dstSet = dummmyOpaqueSceneSet_,
      .binding = 0,
      .type = DescriptorType::CombinedImageSampler,
      .imageInfo = &dummyImage,
      .descriptorCount = 1,
  });
}

void SceneRenderer::UpdateFinalSceneDescriptor() {
  DescriptorImageInfo finalSceneImage{};
  finalSceneImage.imageView = finalScene_.colorTexture.view;
  finalSceneImage.sampler = finalScene_.colorTexture.sampler;
  finalSceneImage.imageLayout = ImageLayout::ShaderReadOnly;

  device_->UpdateDescriptorSet({
      .dstSet = postProcessingSet_,
      .binding = 0,
      .type = DescriptorType::CombinedImageSampler,
      .imageInfo = &finalSceneImage,
      .descriptorCount = 1,
  });
}

void SceneRenderer::EnsureFinalSceneTarget(const FrameRenderContext &frame) {
  bool recreated = false;

  if (finalScene_.colorTexture.image.IsValid() &&
      finalScene_.depthTexture.image.IsValid() &&
      finalScene_.extent.width == frame.extent.width &&
      finalScene_.extent.height == frame.extent.height) {
    return;
  }

  if (finalScene_.colorTexture.image.IsValid() ||
      finalScene_.depthTexture.image.IsValid()) {
    DestroyTexture(device_, finalScene_.colorTexture);
    DestroyTexture(device_, finalScene_.depthTexture);
    finalScene_ = {};
  }

  recreated = true;

  finalScene_.extent = frame.extent;

  finalScene_.colorTexture.image = device_->CreateImage({
      .width = frame.extent.width,
      .height = frame.extent.height,
      .mipLevels = 1,
      .format = colorFormat_,
      .usage = ImageUsage::ColorAttachment | ImageUsage::Sampled |
               ImageUsage::TransferSrc | ImageUsage::TransferDst,
      .debugName = "Opaque Scene Color",
  });

  finalScene_.colorTexture.view = device_->CreateImageView({
      .image = finalScene_.colorTexture.image,
      .format = colorFormat_,
      .aspect = ImageAspect::Color,
      .baseMipLevel = 0,
  });

  finalScene_.colorTexture.sampler = device_->CreateSampler({
      .minFilter = Filter::Linear,
      .magFilter = Filter::Linear,
      .addressU = SamplerAddressMode::ClampToEdge,
      .addressV = SamplerAddressMode::ClampToEdge,
      .addressW = SamplerAddressMode::ClampToEdge,
  });

  finalScene_.depthTexture.image = device_->CreateImage({
      .width = frame.extent.width,
      .height = frame.extent.height,
      .mipLevels = 1,
      .format = depthFormat_,
      .usage = ImageUsage::DepthStencil | ImageUsage::Sampled |
               ImageUsage::TransferSrc | ImageUsage::TransferDst,
      .debugName = "Opaque Scene Color",
  });

  finalScene_.depthTexture.view = device_->CreateImageView({
      .image = finalScene_.depthTexture.image,
      .format = depthFormat_,
      .aspect = ImageAspect::Depth,
      .baseMipLevel = 0,
  });

  finalScene_.depthTexture.sampler = device_->CreateSampler({
      .minFilter = Filter::Linear,
      .magFilter = Filter::Linear,
      .addressU = SamplerAddressMode::ClampToEdge,
      .addressV = SamplerAddressMode::ClampToEdge,
      .addressW = SamplerAddressMode::ClampToEdge,
  });

  if (recreated) {
    UpdateFinalSceneDescriptor();
  }
}

void SceneRenderer::RenderShadowMaps(ICommandList &cmd,
                                     const RenderWorld &world) {
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

  glm::mat4 lightView = glm::lookAtRH(-lightDir * 50.0f, glm::vec3(0.0f), up);

  glm::mat4 lightProj =
      glm::orthoRH_ZO(-15.0f, 15.0f, -15.0f, 15.0f, 1.0f, 80.0f);

  glm::mat4 lightViewProj = lightProj * lightView;

  directionalShadow_.lightViewProj = lightViewProj;

  DepthAttachmentDesc dDesc = {
      .view = directionalShadow_.texture.view,
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

  for (const StaticMeshRenderItem &item : opaques_) {
    if (!item.mesh) {
      continue;
    }

    ShadowPushConstants pc{};
    pc.model = item.worldTransform.ToMatrix() * item.localTransform.ToMatrix();
    pc.lightViewProj = lightViewProj;

    cmd.PushConstants(ShaderStage::Vertex, 0, sizeof(ShadowPushConstants), &pc);

    meshRenderer_.DrawDepthOnly(&cmd, *item.mesh, *item.submesh);
  }

  cmd.EndRendering();
}

void SceneRenderer::BuildStaticMeshRenderList(const RenderWorld &world) {
  opaques_.clear();
  transmissions_.clear();
  alphaBlends_.clear();

  for (const RenderObject &object : world.GetObjects()) {
    if (!object.visible) {
      continue;
    }

    const MeshResource &mesh = world.GetMesh(object.mesh);

    for (const Submesh &submesh : mesh.submeshes) {
      StaticMeshRenderItem item{};
      item.mesh = &mesh;
      item.worldTransform = object.worldTransform;
      item.localTransform = object.localTransform;
      item.objectId = object.objectId;

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

      bool isTransmission =
          material && material->transmission.transmissionFactor > 0.001f;

      bool isBlend = material && material->alphaMode == AlphaMode::Blend;

      if (isTransmission) {
        transmissions_.push_back(item);
      } else if (isBlend) {
        alphaBlends_.push_back(item);
      } else {
        opaques_.push_back(item);
      }
    }
  }
}

void SceneRenderer::RenderOpaqueMeshes(ICommandList &cmd, const Camera &camera,
                                       DebugContext dbgCtx,
                                       DescriptorSetHandle set) {
  RenderStaticMeshes(cmd, camera, dbgCtx, opaques_, set);
}

void SceneRenderer::RenderTransmissionMeshes(ICommandList &cmd,
                                             const Camera &camera,
                                             DebugContext dbgCtx) {
  if (transmissions_.empty()) {
    return;
  }

  PipelineHandle pipeline = GetOrCreateTransmissionPipeline();

  cmd.BindPipeline(pipeline);
  cmd.BindDescriptorSet(pipeline, 1, frameSet_);

  if (iblReady_) {
    cmd.BindDescriptorSet(pipeline, 2, iblResources_.descriptorSet);
  }

  cmd.BindDescriptorSet(pipeline, 3, opaqueSceneSet_);

  DescriptorSetHandle lastMaterialSet{};
  BufferHandle lastVB{};
  BufferHandle lastIB{};

  for (const StaticMeshRenderItem &item : transmissions_) {
    if (!item.mesh || !item.submesh) {
      continue;
    }

    const Submesh &submesh = *item.submesh;
    const MaterialResource *material = item.material;

    DescriptorSetHandle materialSet{};
    if (material && material->descriptorSet.IsValid()) {
      materialSet = material->descriptorSet;
    }

    if (materialSet.id != lastMaterialSet.id) {
      cmd.BindDescriptorSet(pipeline, 0, materialSet);
      lastMaterialSet = materialSet;
    }

    if (item.mesh->vertexBuffer.id != lastVB.id) {
      cmd.BindVertexBuffer(0, item.mesh->vertexBuffer, 0);
      lastVB = item.mesh->vertexBuffer;
    }

    if (item.mesh->indexBuffer.id != lastIB.id) {
      cmd.BindIndexBuffer(item.mesh->indexBuffer, IndexType::U32, 0);
      lastIB = item.mesh->indexBuffer;
    }

    StaticMeshPushConstants pc{};
    pc.model = item.worldTransform.ToMatrix() * item.localTransform.ToMatrix();
    pc.showMode = 4;
    pc.hasTangents = submesh.hasTangents ? 1 : 0;
    pc.materialIndex = 0;

    if (item.materialHandle.IsValid()) {
      auto it = materialGpuIndex_.find(item.materialHandle.id);
      if (it != materialGpuIndex_.end()) {
        pc.materialIndex = static_cast<int>(it->second);
      }
    }

    cmd.PushConstants(ShaderStage::Vertex | ShaderStage::Fragment, 0,
                      sizeof(StaticMeshPushConstants), &pc);

    meshRenderer_.DrawSubmeshBound(&cmd, submesh);
  }
}
void SceneRenderer::RenderAlphaBlendMeshes(ICommandList &cmd,
                                           const Camera &camera,
                                           DebugContext dbgCtx) {
  RenderStaticMeshes(cmd, camera, dbgCtx, alphaBlends_, opaqueSceneSet_);
}

void SceneRenderer::RenderStaticMeshes(
    ICommandList &cmd, const Camera &camera, DebugContext dbgCtx,
    const std::vector<StaticMeshRenderItem> &items,
    DescriptorSetHandle sceneSet) {

  PipelineHandle lastPipeline{};
  DescriptorSetHandle lastMaterialSet{};
  DescriptorSetHandle lastSceneSet{};
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
      cmd.BindDescriptorSet(pipeline, 1, frameSet_);

      if (iblReady_) {
        cmd.BindDescriptorSet(pipeline, 2, iblResources_.descriptorSet);
      }

      lastPipeline = pipeline;
      lastMaterialSet = {};
      lastSceneSet = {};
      lastVertexBuffer = {};
      lastIndexBuffer = {};
    }

    DescriptorSetHandle materialSet{};
    if (material && material->descriptorSet.IsValid()) {
      materialSet = material->descriptorSet;
    }

    if (materialSet.id != lastMaterialSet.id) {
      cmd.BindDescriptorSet(pipeline, 0, materialSet);
      lastMaterialSet = materialSet;
    }

    if (sceneSet.id != lastSceneSet.id) {
      cmd.BindDescriptorSet(pipeline, 3, sceneSet);
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

    StaticMeshPushConstants pc{};
    pc.model = item.worldTransform.ToMatrix() * item.localTransform.ToMatrix();
    pc.showMode = 4;
    pc.hasTangents = submesh.hasTangents ? 1 : 0;
    pc.materialIndex = 0;

    if (item.materialHandle.IsValid()) {
      auto it = materialGpuIndex_.find(item.materialHandle.id);
      if (it != materialGpuIndex_.end()) {
        pc.materialIndex = static_cast<int>(it->second);
      }
    }

    cmd.PushConstants(ShaderStage::Vertex | ShaderStage::Fragment, 0,
                      sizeof(StaticMeshPushConstants), &pc);

    meshRenderer_.DrawSubmeshBound(&cmd, submesh);
  }
}
void SceneRenderer::RenderPostProcessingEffect(ICommandList &cmd,
                                               const FrameRenderContext &frame,
                                               const DebugContext &dbgCtx) {
  PipelineHandle postProcessingPipeline = GetOrCreateTonemappingPipeline();

  cmd.BindPipeline(postProcessingPipeline);

  cmd.BindDescriptorSet(postProcessingPipeline, 0, postProcessingSet_);

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

  if (dbgCtx.drawSceneBounds || dbgCtx.drawMeshBounds) {

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
  frameData.lightViewProj = directionalShadow_.lightViewProj;
  frameData.lightDirection = glm::vec4(directionalShadow_.direction, 0.0f);
  frameData.lightColor = glm::vec4(directionalShadow_.color, 1.0f);
  frameData.lightIntensity = directionalShadow_.intensity;
  frameData.shadowsEnabled = directionalShadow_.enabled ? 1 : 0;

  frameData.proj = camera.GetProjection();
  frameData.view = camera.GetView();

  frameData.viewportSize = {frame.extent.width, frame.extent.height};

  frameData.showMode = static_cast<int>(dbgCtx.mode);

  cmd.UpdateBuffer(
      {.buffer = frameUBO_, .data = &frameData, .size = sizeof(FrameDataGPU)});

  auto currentLayout =
      device_->GetImageLayout(finalScene_.colorTexture.image, 0);

  auto currentDepthLayout =
      device_->GetImageLayout(finalScene_.depthTexture.image, 0);

  ColorAttachmentDesc colorAttachment{};
  colorAttachment.view = finalScene_.colorTexture.view;
  colorAttachment.loadOp = LoadOp::Clear;
  colorAttachment.storeOp = StoreOp::Store;
  colorAttachment.clearValue = {1.0f, 0.0f, 0.0f, 1.0f};

  DepthAttachmentDesc depthAttachment{};
  depthAttachment.view = finalScene_.depthTexture.view;
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
  frameData.lightViewProj = directionalShadow_.lightViewProj;
  frameData.lightDirection = glm::vec4(directionalShadow_.direction, 0.0f);
  frameData.lightColor = glm::vec4(directionalShadow_.color, 1.0f);
  frameData.lightIntensity = directionalShadow_.intensity;
  frameData.shadowsEnabled = directionalShadow_.enabled ? 1 : 0;

  frameData.viewportSize = {frame.extent.width, frame.extent.height};

  frameData.proj = camera.GetProjection();
  frameData.view = camera.GetView();

  frameData.showMode = static_cast<int>(dbgCtx.mode);

  cmd.UpdateBuffer(
      {.buffer = frameUBO_, .data = &frameData, .size = sizeof(FrameDataGPU)});

  auto currentLayout = device_->GetImageLayout(opaqueScene_.texture.image, 0);

  auto currentDepth = device_->GetImageLayout(frame.depthImage, 0);

  ColorAttachmentDesc colorAttachment{};
  colorAttachment.view = opaqueScene_.renderView;
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
  auto currentLayout =
      device_->GetImageLayout(finalScene_.colorTexture.image, 0);

  auto currentLayoutBB = device_->GetImageLayout(frame.backbufferImage, 0);

  auto currentDepthLayout = device_->GetImageLayout(frame.depthImage, 0);

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
    gpu.ior = 1.5f;
    gpu.attenuationColorDistance = glm::vec4(
        material.volume.attenuationColor, material.volume.attenuationDistance);

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

} // namespace Rodan
