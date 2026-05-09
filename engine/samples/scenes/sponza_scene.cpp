#include "sponza_scene.h"

#include "glm/geometric.hpp"
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

  camera_.SetPerspective(60.0f, 16.0f / 9.0f, 0.1f, 500.0f);

  LoadScene(device_);

  sceneRenderer_.Initialize(device_, swapchain_, colorFormat, depthFormat,
                            asset_->GetMaterialLayout());

  sunLight_ = renderWorld_.AddDirectionalLight({
      .direction = glm::normalize(glm::vec3(0.4f, -1.0f, 0.3f)),
      .color = glm::vec3(1.0f),
      .intensity = 4.0f,
      .castsShadow = true,
  });
}

void SponzaScene::Shutdown(IDevice *device) {
  if (!device) {
    return;
  }

  sceneRenderer_.Shutdown(device);

  instances_.clear();

  if (asset_) {
    asset_->Destroy(device);
    asset_.reset();
  }

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

void SponzaScene::Render(ICommandList &cmd, const FrameRenderContext &frame) {
  sceneRenderer_.Render(cmd, renderWorld_, camera_, frame);
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

  const char *items[] = {"BaseColor", "Normal", "MetallicRoughness", "Tangent",
                         "Final"};
  int mode = static_cast<int>(showMode_);
  if (ImGui::Combo("Show", &mode, items, IM_ARRAYSIZE(items))) {
    showMode_ = static_cast<Show>(mode);
  }
  ImGui::Text("Mode: %s", items[mode]);

  ImGui::Text("Hold RMB to look around");

  DirectionalLight &sun = renderWorld_.GetDirectionalLight(sunLight_);

  glm::vec3 dir = sun.direction;

  if (ImGui::DragFloat3("Sun Direction", &dir.x, 0.01f, -1.0f, 1.0f)) {
    if (glm::length(dir) > 0.0001f) {
      sun.direction = glm::normalize(dir);
    }
  }

  ImGui::DragFloat("Sun Intensity", &sun.intensity, 0.1f, 0.0f, 20.0f);
  ImGui::Checkbox("Cast Shadows", &sun.castsShadow);
  ImGui::End();
}

void SponzaScene::LoadScene(IDevice *device) {
  auto upload = device->CreateUploadContext(4 * 256 * 1024 * 1024);
  upload->Begin();

  asset_ = StaticGltfAsset::Load(
      device, upload.get(),
      Velos::Path::Resolve("assets/models/sponza/Sponza.gltf").string());

  upload->Flush();

  instances_ = asset_->GetInstances();

  if (instances_.empty()) {
    throw std::runtime_error("Sponza scene loaded, but produced no instances");
  }

  std::vector<MaterialHandle> materialHandles;
  materialHandles.reserve(asset_->GetMaterials().size());

  for (const MaterialResource &material : asset_->GetMaterials()) {
    materialHandles.push_back(renderWorld_.AddMaterial(material));
  }

  for (const StaticMeshInstance &instance : instances_) {
    if (!instance.mesh) {
      continue;
    }

    MeshHandle meshHandle = renderWorld_.AddMesh(*instance.mesh);

    RenderObjectDesc desc{};
    desc.mesh = meshHandle;
    desc.materials = materialHandles;
    desc.transform = instance.localTransform;
    desc.visible = true;

    renderWorld_.CreateObject(desc);
  }
}

} // namespace Rodan
