#include "metal_roughness_spheres_no_tex_scene.h"

#include "graphics/shaders_types.h"
#include "imgui.h"
#include "shader/shader_compiler.h"

#include <glm/gtc/matrix_transform.hpp>
#include <path.h>
#include <stdexcept>

namespace Rodan {

using namespace Velos;
using namespace Velos::RHI;

void MetalRoughnessSpheresNoTexScene::Initialize(IDevice *device,
                                                 SwapchainHandle swapchain,
                                                 Format colorFormat,
                                                 Format depthFormat) {
  device_ = device;
  swapchain_ = swapchain;
  colorFormat_ = colorFormat;
  depthFormat_ = depthFormat;

  camera_.SetPerspective(60.0f, 16.0f / 9.0f, 0.1f, 500.0f);

  // IMPORTANT: load asset before pipeline (we need descriptor layouts)
  LoadScene(device_);

  glm::mat4 S = glm::scale(glm::mat4(1.0f), glm::vec3(1000.0f));
  for (auto &instance : instances_) {
    instance.localTransform = S * instance.localTransform;
  }

  CreatePipeline(device_);
}

void MetalRoughnessSpheresNoTexScene::Shutdown(IDevice *device) {
  if (!device) {
    return;
  }

  materialPtrs_.clear();
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

void MetalRoughnessSpheresNoTexScene::OnResize(IDevice *device, u32 width,
                                               u32 height) {
  (void)device;
  (void)width;
  (void)height;
}

void MetalRoughnessSpheresNoTexScene::Update(float deltaSeconds,
                                             const SceneUpdateContext &ctx) {
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

void MetalRoughnessSpheresNoTexScene::Prepare(ICommandList &cmd) {
  if (asset_) {
    asset_->Prepare(cmd);
  }
}

void MetalRoughnessSpheresNoTexScene::Render(ICommandList &cmd) {
  drawInstanceCount_ = static_cast<u32>(instances_.size());
  drawSubmeshCount_ = 0;

  cmd.BindPipeline(pipeline_);

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

    meshRenderer_.Draw(&cmd, mesh, materialPtrs_, pipeline_);
  }
}
void MetalRoughnessSpheresNoTexScene::RenderImGui() {
  ImGui::Begin("Metal Roughness Scene");
  ImGui::Text("Static glTF scene test");
  ImGui::Separator();
  ImGui::Text("Instances: %u", drawInstanceCount_);
  ImGui::Text("Submeshes drawn: %u", drawSubmeshCount_);
  ImGui::Text("Hold RMB to look around");
  ImGui::End();
}

void MetalRoughnessSpheresNoTexScene::CreatePipeline(IDevice *device) {
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
      .debugName = "Metal Roughness Vertex Shader",
  });

  fragmentShader_ = device->CreateShader({
      .stage = ShaderStage::Fragment,
      .bytecode = fragSpv.spirv.data(),
      .bytecodeSize =
          static_cast<u64>(fragSpv.spirv.size() * sizeof(std::uint32_t)),
      .entryPoint = "main",
      .reflection = fragSpv.reflection,
      .debugName = "Metal Roughness Fragment Shader",
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
  pipelineDesc.raster.cullBackFaces = false;
  pipelineDesc.raster.frontFaceCCW = true;
  pipelineDesc.raster.wireframe = false;
  pipelineDesc.blend = {.enable = false};
  pipelineDesc.colorFormat = colorFormat_;
  pipelineDesc.depth = {
      .depthTestEnable = true,
      .depthWriteEnable = true,
      .depthFormat = depthFormat_,
  };
  pipelineDesc.debugName = "Metal Roughness Scene Pipeline";

  pipeline_ = device->CreateGraphicsPipeline(pipelineDesc);
}

void MetalRoughnessSpheresNoTexScene::LoadScene(IDevice *device) {
  auto upload = device->CreateUploadContext(64 * 1024 * 1024);
  upload->Begin();

  asset_ = StaticGltfAsset::Load(
      device, upload.get(),
      Velos::Path::Resolve("assets/models/metal-roughness-spheres-no-textures/"
                           "MetalRoughSpheresNoTextures.gltf")
          .string());

  upload->Flush();

  instances_ = asset_->GetInstances();

  if (instances_.empty()) {
    throw std::runtime_error(
        "Metal Roughness scene loaded, but produced no instances");
  }

  materialPtrs_.clear();
  materialPtrs_.reserve(asset_->GetMaterials().size());

  for (const MaterialResource &material : asset_->GetMaterials()) {
    materialPtrs_.push_back(&material);
  }
}

} // namespace Rodan
