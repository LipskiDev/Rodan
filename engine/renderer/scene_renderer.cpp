#include "glm/ext/matrix_clip_space.hpp"
#include "glm/ext/matrix_transform.hpp"
#include "graphics/material_types.h"
#include "graphics/mesh_resource.h"
#include "graphics/shaders_types.h"
#include "graphics/texture.h"
#include "rhi/rhi_command_list.h"
#include "rhi/rhi_handles.h"
#include "rhi/rhi_pipeline.h"
#include "rhi/rhi_resources.h"
#include "rhi/rhi_types.h"
#include "scene/render_world.h"
#include <core/path.h>
#include <renderer/scene_renderer.h>
#include <stdexcept>

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

  directionalShadow_.texture.image = device->CreateImage({
      .width = 2048,
      .height = 2048,
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
       .visibility = ShaderStage::Vertex | ShaderStage::Fragment}};

  DescriptorPoolSize poolSizes[] = {
      {
          .type = DescriptorType::CombinedImageSampler,
          .count = 1,
      },
      {
          .type = DescriptorType::UniformBuffer,
          .count = 1,
      }};

  frameDescriptorPool_ = device_->CreateDescriptorPool({
      .poolSizes = poolSizes,
      .poolSizeCount = 2,
      .maxSets = 1,
  });

  frameLayout_ = device_->CreateDescriptorSetLayout({
      .bindings = frameBindings,
      .bindingCount = 2,
      .debugName = "SceneRenderer Frame Descriptor Set Layout",
  });

  frameSet_ =
      device_->AllocateDescriptorSet(frameDescriptorPool_, frameLayout_);

  frameUBO_ = device_->CreateBuffer({.size = sizeof(FrameDataGPU),
                                     .usage = BufferUsage::Uniform,
                                     .memoryUsage = MemoryUsage::CPUToGPU,
                                     .debugName = "Scene Frame UBO"});

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
}

void SceneRenderer::Shutdown(IDevice *device) {
  for (auto &[key, pipeline] : pipelines_) {
    device->DestroyPipeline(pipeline);
  }
  pipelines_.clear();

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

  device_->DestroySampler(directionalShadow_.texture.sampler);
  device_->DestroyImageView(directionalShadow_.texture.view);
  device_->DestroyImage(directionalShadow_.texture.image);

  device->DestroyBuffer(frameUBO_);
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
  BuildStaticMeshRenderList(world);

  RenderShadowMaps(cmd, world);

  BeginMainPass(cmd, frame, camera);

  RenderStaticMeshes(cmd, camera);

  RenderDebug(cmd, camera, dbgCtx);

  if (frame.renderUi) {
    frame.renderUi(cmd);
  }

  EndMainPass(cmd);
  staticMeshes_.clear();
}

void SceneRenderer::SubmitStaticMesh(StaticMeshRenderItem item) {
  staticMeshes_.push_back(item);
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
  DescriptorSetLayoutHandle setLayouts[] = {materialLayout_, frameLayout_};
  desc.layout.descriptorSetLayouts = setLayouts;
  desc.layout.descriptorSetLayoutCount = 2;

  // Raster state
  desc.raster.cullBackFaces = false;
  desc.raster.frontFaceCCW = true;
  desc.raster.wireframe = false;

  // Depth state
  desc.depth.depthTestEnable = true;
  desc.depth.depthWriteEnable = true;
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

void SceneRenderer::EnsureShadowMapReadable(ICommandList &cmd) {
  if (shadowLayout_ != ImageLayout::ShaderReadOnly) {
    cmd.Barrier({
        .image = directionalShadow_.texture.image,
        .oldLayout = shadowLayout_,
        .newLayout = ImageLayout::ShaderReadOnly,
        .aspect = ImageAspect::Depth,
    });

    shadowLayout_ = ImageLayout::ShaderReadOnly;
  }
}

void SceneRenderer::RenderShadowMaps(ICommandList &cmd,
                                     const RenderWorld &world) {
  directionalShadow_.enabled = false;

  const auto &lights = world.GetDirectionalLights();
  if (lights.empty()) {
    EnsureShadowMapReadable(cmd);
    return;
  }

  const DirectionalLight &light = lights[0];

  if (!light.castsShadow) {
    EnsureShadowMapReadable(cmd);
    return;
  }

  directionalShadow_.enabled = true;

  cmd.Barrier({
      .image = directionalShadow_.texture.image,
      .oldLayout = shadowLayout_,
      .newLayout = ImageLayout::DepthAttachment,
      .aspect = ImageAspect::Depth,
  });

  shadowLayout_ = ImageLayout::DepthAttachment;

  glm::vec3 lightDir = glm::normalize(light.direction);

  glm::vec3 up = glm::vec3(0, 1, 0);

  if (std::abs(glm::dot(lightDir, up)) > 0.99f) {
    up = glm::vec3(0, 0, 1);
  }

  glm::mat4 lightView = glm::lookAtRH(-lightDir * 50.0f, glm::vec3(0.0f), up);

  glm::mat4 lightProj = glm::orthoRH_ZO(-15.f, 15.f, -15.f, 15.f, 1.f, 80.f);

  glm::mat4 lightViewProj = lightProj * lightView;

  directionalShadow_.lightViewProj = lightViewProj;

  DepthAttachmentDesc dDesc = {
      .view = directionalShadow_.texture.view,
      .loadOp = LoadOp::Clear,
      .storeOp = StoreOp::Store,
      .clearDepth = 1.0f,
  };

  cmd.BeginRendering({
      .renderArea = {.offset = {0, 0}, .extent = {2048, 2048}},
      .colorAttachments = nullptr,
      .colorAttachmentCount = 0,
      .depthAttachment = &dDesc,
  });

  cmd.SetViewport({
      .x = 0.0f,
      .y = 0.0f,
      .width = 2048.0f,
      .height = 2048.0f,
      .minDepth = 0.0f,
      .maxDepth = 1.0f,
  });

  cmd.SetScissor({
      .offset = {0, 0},
      .extent = {2048, 2048},
  });

  PipelineHandle pipeline = GetOrCreateShadowPipeline();
  cmd.BindPipeline(pipeline);

  for (const StaticMeshRenderItem &item : staticMeshes_) {
    if (!item.mesh) {
      continue;
    }

    ShadowPushConstants pc{};
    pc.model = item.worldTransform.ToMatrix() * item.localTransform.ToMatrix();
    pc.lightViewProj = lightViewProj;

    cmd.PushConstants(ShaderStage::Vertex, 0, sizeof(ShadowPushConstants), &pc);

    meshRenderer_.DrawDepthOnly(&cmd, *item.mesh);
  }

  cmd.EndRendering();

  cmd.Barrier({
      .image = directionalShadow_.texture.image,
      .oldLayout = ImageLayout::DepthAttachment,
      .newLayout = ImageLayout::ShaderReadOnly,
      .aspect = ImageAspect::Depth,
  });

  shadowLayout_ = ImageLayout::ShaderReadOnly;
}

void SceneRenderer::BuildStaticMeshRenderList(const RenderWorld &world) {
  staticMeshes_.clear();

  for (const RenderObject &object : world.GetObjects()) {
    if (!object.visible) {
      continue;
    }

    StaticMeshRenderItem item{};
    item.mesh = &world.GetMesh(object.mesh);
    item.worldTransform = object.worldTransform;
    item.localTransform = object.localTransform;
    item.objectId = object.objectId;

    item.materials.reserve(object.materials.size());
    for (MaterialHandle materialHandle : object.materials) {
      item.materials.push_back(&world.GetMaterial(materialHandle));
    }

    staticMeshes_.push_back(std::move(item));
  }

  if (!world.GetDirectionalLights().empty()) {
    DirectionalLight light = world.GetDirectionalLights()[0];
    directionalShadow_.direction = light.direction;
    directionalShadow_.color = light.color;
    directionalShadow_.intensity = light.intensity;
  }
}

void SceneRenderer::RenderStaticMeshes(ICommandList &cmd,
                                       const Camera &camera) {
  for (const StaticMeshRenderItem &item : staticMeshes_) {
    if (!item.mesh) {
      continue;
    }

    for (const Submesh &submesh : item.mesh->submeshes) {
      const MaterialResource *material = nullptr;

      if (submesh.materialSlot < item.materials.size()) {
        material = item.materials[submesh.materialSlot];
      }

      MeshPipelineKey key{};
      if (material) {
        key.alphaMode = material->alphaMode;
        key.doubleSided = material->doubleSided;
      }

      PipelineHandle pipeline = GetOrCreatePipeline(key);

      cmd.BindPipeline(pipeline);
      cmd.BindDescriptorSet(pipeline, 1, frameSet_);

      StaticMeshPushConstants pc{};
      pc.model =
          item.worldTransform.ToMatrix() * item.localTransform.ToMatrix();
      pc.baseColorFactor =
          material ? material->baseColorFactor : glm::vec4(1.0f);
      pc.metallicFactor = material ? material->metallicFactor : 0.0f;
      pc.roughnessFactor = material ? material->roughnessFactor : 1.0f;
      pc.alphaCutoff = material ? material->alphaCutoff : 0.5f;
      pc.showMode = 4;
      pc.hasMaterial = material ? 1 : 0;
      pc.alphaMode = material ? static_cast<int>(material->alphaMode) : 0;
      pc.hasTangents = submesh.hasTangents ? 1 : 0;

      cmd.PushConstants(ShaderStage::Vertex | ShaderStage::Fragment, 0,
                        sizeof(StaticMeshPushConstants), &pc);

      meshRenderer_.DrawSubmesh(&cmd, *item.mesh, submesh, material, pipeline);
    }
  }
}

void SceneRenderer::RenderDebug(ICommandList &cmd, const Camera &camera,
                                DebugContext dbgCtx) {
  if (!lineRenderer3D_) {
    return;
  }

  lineRenderer3D_->clear();

  const glm::vec4 sceneBoundsColor{1.0f, 1.0f, 1.0f, 0.8f};
  const glm::vec4 submeshBoundsColor{1.0f, 1.0f, 0.0f, 0.8f};

  for (const auto &mesh : staticMeshes_) {
    const glm::mat4 model =
        mesh.worldTransform.ToMatrix() * mesh.localTransform.ToMatrix();

    if (dbgCtx.drawSceneBounds) {
      AABB aabb = mesh.mesh->aabb.Transform(model);
      lineRenderer3D_->aabb(aabb.lower, aabb.upper, sceneBoundsColor);
    }

    if (dbgCtx.drawMeshBounds) {
      for (const auto &submesh : mesh.mesh->submeshes) {
        AABB aabb = submesh.aabb.Transform(model);
        lineRenderer3D_->aabb(aabb.lower, aabb.upper, submeshBoundsColor);
      }
    }
  }

  lineRenderer3D_->render(cmd, camera.GetProjection() * camera.GetView());
  lineRenderer3D_->clear();
}

void SceneRenderer::BeginMainPass(ICommandList &cmd,
                                  const FrameRenderContext &frame,
                                  const Camera &camera) {

  FrameDataGPU frameData{};
  frameData.lightViewProj = directionalShadow_.lightViewProj;
  frameData.lightDirection = directionalShadow_.direction;
  frameData.lightColor = directionalShadow_.color;
  frameData.lightIntensity = directionalShadow_.intensity;
  frameData.renderShadows = directionalShadow_.enabled ? 1.0f : 0.0f;

  frameData.proj = camera.GetProjection();
  frameData.view = camera.GetView();

  cmd.UpdateBuffer(
      {.buffer = frameUBO_, .data = &frameData, .size = sizeof(FrameDataGPU)});

  cmd.Barrier({
      .image = frame.backbufferImage,
      .newLayout = ImageLayout::ColorAttachment,
      .aspect = ImageAspect::Color,
  });

  cmd.Barrier({
      .image = frame.depthImage,
      .newLayout = ImageLayout::DepthAttachment,
      .aspect = ImageAspect::Depth,
  });

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

} // namespace Rodan
