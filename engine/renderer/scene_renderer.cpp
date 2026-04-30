#include "graphics/material_types.h"
#include "graphics/mesh_resource.h"
#include "graphics/shaders_types.h"
#include "rhi/rhi_handles.h"
#include "rhi/rhi_pipeline.h"
#include "rhi/rhi_types.h"
#include "scene/static_mesh_instance.h"
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
}

void SceneRenderer::Shutdown(IDevice *device) {
  for (auto &[key, pipeline] : pipelines_) {
    device->DestroyPipeline(pipeline);
  }
  pipelines_.clear();

  if (staticMeshVS_) {
    device->DestroyShader(staticMeshVS_);
    staticMeshVS_ = {};
  }

  if (staticMeshFS_) {
    device->DestroyShader(staticMeshFS_);
    staticMeshFS_ = {};
  }

  graphRenderer_.reset();
  lineRenderer3D_.reset();
  lineRenderer2D_.reset();

  staticMeshes_.clear();

  device_ = nullptr;
  swapchain_ = {};
}

void SceneRenderer::Render(ICommandList &cmd, const Camera &camera) {

  for (const StaticMeshRenderItem &item : staticMeshes_) {
    if (!item.instance || !item.instance->mesh || !item.materials) {
      continue;
    }

    const StaticMeshInstance &instance = *item.instance;
    const MeshResource &mesh = *instance.mesh;

    MVPPushConstants push{};
    push.model = item.world;
    push.view = camera.GetView();
    push.proj = camera.GetProjection();
    push.showMode = 4;

    for (const Submesh &submesh : mesh.submeshes) {
      const MaterialResource *material = item.materialOverride;
      if (!material) {
        if (submesh.materialSlot >= 0 &
            static_cast<size_t>(submesh.materialSlot) <
                item.materials->size()) {
          material = &(*item.materials)[submesh.materialSlot];
        }
      }

      MeshPipelineKey key{};
      if (material) {
        key.alphaMode = material->alphaMode;
        key.doubleSided = material->doubleSided;
      }

      PipelineHandle pipeline = GetOrCreatePipeline(key);

      cmd.BindPipeline(pipeline);
      cmd.PushConstants(ShaderStage::Vertex | ShaderStage::Fragment, 0,
                        sizeof(MVPPushConstants), &push);

      meshRenderer_.DrawSubmesh(&cmd, instance, submesh, material, pipeline);
    }
  }

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
  DescriptorSetLayoutHandle setLayouts[] = {materialLayout_};
  desc.layout.descriptorSetLayouts = setLayouts;
  desc.layout.descriptorSetLayoutCount = 1;

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

} // namespace Rodan
