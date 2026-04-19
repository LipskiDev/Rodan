#include "sponza_scene.h"

#include "imgui.h"
#include "rhi/rhi_types.h"
#include "shader/shader_compiler.h"

#include <glm/gtc/matrix_transform.hpp>
#include <path.h>
#include <stdexcept>

namespace Rodan {

using namespace Velos;
using namespace Velos::RHI;

void SponzaScene::Initialize(IDevice *device, SwapchainHandle swapchain,
                             Format colorFormat, Format depthFormat) {
  device_ = device;
  swapchain_ = swapchain;
  colorFormat_ = colorFormat;
  depthFormat_ = depthFormat;

  camera_.SetPerspective(60.0f, 16.0f / 9.0f, 0.1f, 500.0f);

  LoadScene(device_);
  CreatePipeline(device_);
}

void SponzaScene::Shutdown(IDevice *device) {
  if (!device) {
    return;
  }

  instances_.clear();

  if (asset_) {
    asset_->Destroy(device);
    asset_.reset();
  }

  device->DestroyPipeline(pipeline_);
  device->DestroyShader(fragmentShader_);
  device->DestroyShader(vertexShader_);

  pipeline_ = {};
  fragmentShader_ = {};
  vertexShader_ = {};

  device_ = nullptr;
  swapchain_ = {};
}

void SponzaScene::OnResize(IDevice *device, u32 width, u32 height) {
  (void)device;
  (void)width;
  (void)height;
}

void SponzaScene::Update(float deltaSeconds, const SceneUpdateContext &ctx) {
  if (ctx.framebufferWidth == 0 || ctx.framebufferHeight == 0) {
    return;
  }

  camera_.SetPerspective(60.0f,
                         static_cast<float>(ctx.framebufferWidth) /
                             static_cast<float>(ctx.framebufferHeight),
                         0.1f, 500.0f);

  if (ctx.input) {
    for (const InputEvent &event : ctx.input->GetEvents()) {
      if (event.type == InputEventType::KeyDown ||
          event.type == InputEventType::KeyUp) {
        camera_.OnKeyboard(event);
      } else if (event.type == InputEventType::MouseMove) {
        float dx = 0.0f;
        float dy = 0.0f;

        if (firstMouse_) {
          lastMouseX_ = event.mouseMove.x;
          lastMouseY_ = event.mouseMove.y;
          firstMouse_ = false;
        } else {
          dx = event.mouseMove.x - lastMouseX_;
          dy = event.mouseMove.y - lastMouseY_;
          lastMouseX_ = event.mouseMove.x;
          lastMouseY_ = event.mouseMove.y;
        }

        if (ctx.input->IsMouseDown(MouseButton::Right)) {
          camera_.OnMouseMove(dx, dy);
        }
      }
    }
  }

  camera_.Update(deltaSeconds);
}

void SponzaScene::Prepare(ICommandList &cmd) { (void)cmd; }

void SponzaScene::Render(ICommandList &cmd) {
  drawInstanceCount_ = static_cast<u32>(instances_.size());
  drawSubmeshCount_ = 0;

  cmd.BindPipeline(pipeline_);

  const auto &materials = asset_->GetMaterials();

  for (const StaticMeshInstance &instance : instances_) {
    if (!instance.mesh) {
      continue;
    }

    MVPPushConstants push{};
    push.model = instance.localTransform;
    push.view = camera_.GetView();
    push.proj = camera_.GetProjection();

    cmd.PushConstants(ShaderStage::Vertex | ShaderStage::Fragment, 0,
                      sizeof(MVPPushConstants), &push);

    const MeshResource &mesh = *instance.mesh;
    drawSubmeshCount_ += static_cast<u32>(mesh.submeshes.size());

    meshRenderer_.Draw(&cmd, instance, materials, pipeline_);
  }
}

void SponzaScene::RenderImGui() {
  ImGui::Begin("Sponza Scene");
  ImGui::Text("Static glTF scene test");
  ImGui::Separator();

  if (asset_) {
    ImGui::Text("Instances: %u", drawInstanceCount_);
    ImGui::Text("Submeshes drawn: %u", drawSubmeshCount_);
    ImGui::Text("Materials: %u",
                static_cast<u32>(asset_->GetMaterials().size()));
  }

  ImGui::Text("Hold RMB to look around");
  ImGui::End();
}

void SponzaScene::CreatePipeline(IDevice *device) {
  auto vertSpv = ShaderCompiler::CompileFile({
      .path = Velos::Path::Resolve("assets/shaders/static_mesh.vert").string(),
      .stage = ShaderStage::Vertex,
      .entryPoint = "main",
  });

  auto fragSpv = ShaderCompiler::CompileFile({
      .path = Velos::Path::Resolve("assets/shaders/static_mesh.frag").string(),
      .stage = ShaderStage::Fragment,
      .entryPoint = "main",
  });

  vertexShader_ = device->CreateShader({
      .stage = ShaderStage::Vertex,
      .bytecode = vertSpv.spirv.data(),
      .bytecodeSize =
          static_cast<u64>(vertSpv.spirv.size() * sizeof(std::uint32_t)),
      .entryPoint = "main",
      .reflection = vertSpv.reflection,
      .debugName = "Sponza Vertex Shader",
  });

  fragmentShader_ = device->CreateShader({
      .stage = ShaderStage::Fragment,
      .bytecode = fragSpv.spirv.data(),
      .bytecodeSize =
          static_cast<u64>(fragSpv.spirv.size() * sizeof(std::uint32_t)),
      .entryPoint = "main",
      .reflection = fragSpv.reflection,
      .debugName = "Sponza Fragment Shader",
  });

  VertexBufferLayoutDesc layout{
      .stride = sizeof(ImportedVertex),
      .inputRate = VertexInputRate::PerVertex,
      .attributes = {{
                         .location = 0,
                         .format = VertexFormat::Float32x3,
                         .offset = offsetof(ImportedVertex, position),
                     },
                     {
                         .location = 1,
                         .format = VertexFormat::Float32x3,
                         .offset = offsetof(ImportedVertex, normal),
                     },
                     {
                         .location = 2,
                         .format = VertexFormat::Float32x2,
                         .offset = offsetof(ImportedVertex, uv),
                     }},
  };

  DescriptorSetLayoutHandle setLayouts[] = {asset_->GetMaterialLayout()};

  GraphicsPipelineDesc pipelineDesc{};
  pipelineDesc.vertexShader = vertexShader_;
  pipelineDesc.fragmentShader = fragmentShader_;
  pipelineDesc.vertexLayouts.push_back(layout);
  pipelineDesc.layout.descriptorSetLayouts = setLayouts;
  pipelineDesc.layout.descriptorSetLayoutCount = 1;

  pipelineDesc.topology = PrimitiveTopology::TriangleList;
  pipelineDesc.raster.cullBackFaces = true;
  pipelineDesc.raster.frontFaceCCW = true;
  pipelineDesc.raster.wireframe = false;
  pipelineDesc.blend = {
      .enable = true,

      .srcColor = BlendFactor::SrcAlpha,
      .dstColor = BlendFactor::OneMinusSrcAlpha,
      .colorOp = BlendOp::Add,

      .srcAlpha = BlendFactor::One,
      .dstAlpha = BlendFactor::OneMinusSrcAlpha,
      .alphaOp = BlendOp::Add,
  };
  pipelineDesc.colorFormat = colorFormat_;
  pipelineDesc.depth = {
      .depthTestEnable = true,
      .depthWriteEnable = true,
      .depthFormat = depthFormat_,
  };
  pipelineDesc.debugName = "Sponza Scene Pipeline";

  pipeline_ = device->CreateGraphicsPipeline(pipelineDesc);
}

void SponzaScene::LoadScene(IDevice *device) {
  auto upload = device->CreateUploadContext(256 * 1024 * 1024);
  upload->Begin();

  asset_ = StaticGltfAsset::Load(
      device, upload.get(),
      Velos::Path::Resolve("assets/models/sponza/Sponza.gltf").string());

  upload->Flush();

  instances_ = asset_->GetInstances();

  if (instances_.empty()) {
    throw std::runtime_error("Sponza scene loaded, but produced no instances");
  }
}

} // namespace Rodan
