#include <chrono>
#include <iostream>
#include <samples/scenes/gltf_viewer_scene.h>

#include <nfd.h>

#include "glm/geometric.hpp"
#include "imgui.h"
#include "imgui_internal.h"
#include "rhi/rhi_types.h"

#include <glm/gtc/matrix_transform.hpp>
#include <path.h>
#include <stdexcept>

namespace Rodan {

using namespace Velos;
using namespace Velos::RHI;

void GltfViewerScene::Initialize(IDevice *device, SwapchainHandle swapchain,
                                 Format colorFormat, Format depthFormat) {

  device_ = device;
  swapchain_ = swapchain;

  if (NFD_Init() != NFD_OKAY) {
    std::cerr << "Failed to initialize NFD: " << NFD_GetError() << std::endl;
  }

  camera_.SetPerspective(60.0f, 16.0f / 9.0f, 0.1f, 500.0f);

  LoadScene(device_, currentScenePath_);

  sceneRenderer_.Initialize(device_, swapchain_, colorFormat, depthFormat,
                            asset_->GetMaterialLayout());

  sunLight_ = renderWorld_.AddDirectionalLight({
      .direction = glm::normalize(glm::vec3(0.4f, -1.0f, 0.3f)),
      .color = glm::vec3(1.0f),
      .intensity = 4.0f,
      .castsShadow = true,
  });
}

void GltfViewerScene::Shutdown(IDevice *device) {
  if (!device) {
    return;
  }

  sceneRenderer_.Shutdown(device);

  instances_.clear();

  if (asset_) {
    asset_->Destroy(device);
    asset_.reset();
  }

  NFD_Quit();

  device_ = nullptr;
  swapchain_ = {};
}

void GltfViewerScene::OnResize(IDevice *device, u32 width, u32 height) {
  (void)device;
  (void)width;
  (void)height;
}

void GltfViewerScene::Update(float deltaSeconds,
                             const SceneUpdateContext &ctx) {
  if (ctx.framebufferWidth == 0 || ctx.framebufferHeight == 0) {
    return;
  }

  if (pendingReload_) {
    pendingReload_ = false;
    renderWorld_.Clear();

    try {
      ReloadScene(pendingScenePath_);
      currentScenePath_ = pendingScenePath_;

      std::cout << "Loaded scene: " << currentScenePath_ << std::endl;

    } catch (const std::exception &e) {
      std::cerr << "Failed to load scene: " << e.what() << std::endl;
    }
  }

  if (changed) {
    renderWorld_.SetTransform(currentRenderTarget_, currentTransform_);
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

void GltfViewerScene::Prepare(ICommandList &cmd) { (void)cmd; }

void GltfViewerScene::Render(ICommandList &cmd,
                             const FrameRenderContext &frame) {
  sceneRenderer_.Render(cmd, renderWorld_, camera_, frame);
}

void GltfViewerScene::RenderImGui() {
  ImGui::Begin("GltfViewer Scene");
  ImGui::Text("Static glTF scene test");
  ImGui::Separator();

  ImGui::Text("Loaded file:");
  ImGui::TextWrapped("%s", currentScenePath_.c_str());

  if (ImGui::Button("Open glTF / GLB")) {
    nfdu8char_t *outPath = nullptr;

    nfdu8filteritem_t filters[] = {
        {"glTF files", "gltf,glb"},
    };

    nfdopendialogu8args_t args = {};
    args.filterList = filters;
    args.filterCount = 1;

    nfdresult_t result = NFD_OpenDialogU8_With(&outPath, &args);

    if (result == NFD_OKAY) {
      std::string selectedPath = outPath;
      NFD_FreePathU8(outPath);

      if (IsGltfPath(selectedPath)) {
        pendingScenePath_ = selectedPath;
        pendingReload_ = true;
      }
    } else if (result == NFD_CANCEL) {
      // User cancelled; do nothing.
    } else {
      std::cerr << "NFD error: " << NFD_GetError() << std::endl;
    }
  }

  ImGui::Separator();
  ImGui::Text("Model Transform");

  changed |=
      ImGui::DragFloat3("Position", &currentTransform_.position.x, 0.01f);

  glm::vec3 euler = glm::degrees(glm::eulerAngles(currentTransform_.rotation));

  if (ImGui::DragFloat3("Rotation", &euler.x, 0.5f, -360.0f, 360.0f)) {
    currentTransform_.rotation = glm::quat(glm::radians(euler));
    changed = true;
  }

  ImGui::Checkbox("Lock Uniform Scale", &lockUniformScale_);

  if (lockUniformScale_) {
    float uniformScale = currentTransform_.scale.x;

    if (ImGui::DragFloat("Scale", &uniformScale, 0.01f, 0.001f, 100.0f)) {
      currentTransform_.scale = glm::vec3(uniformScale);

      changed = true;
    }
  } else {
    changed |= ImGui::DragFloat3("Scale", &currentTransform_.scale.x, 0.01f,
                                 0.001f, 100.0f);
  }

  if (ImGui::Button("Reset Transform")) {
    currentTransform_ = Rodan::Transform{};
    changed = true;
  }

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

void GltfViewerScene::LoadScene(IDevice *device, std::string path) {
  auto upload = device->CreateUploadContext(4 * 256 * 1024 * 1024);
  upload->Begin();

  asset_ =
      StaticGltfAsset::Load(device, upload.get(), Velos::Path::Resolve(path));

  upload->Flush();

  instances_ = asset_->GetInstances();

  if (instances_.empty()) {
    throw std::runtime_error(
        "GltfViewer scene loaded, but produced no instances");
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
    desc.visible = true;

    Transform transform = instance.localTransform;

    desc.transform = transform;

    currentRenderTarget_ = renderWorld_.CreateObject(desc);
    printf("NEW RENDER TARGET: %d\n", currentRenderTarget_.id);
  }
}

void GltfViewerScene::ReloadScene(const std::string &path) {
  if (!device_) {
    return;
  }

  device_->WaitIdle(); // or your RHI equivalent

  renderWorld_.Clear();

  instances_.clear();

  if (asset_) {
    asset_->Destroy(device_);
    asset_.reset();
  }

  LoadScene(device_, path);

  sunLight_ = renderWorld_.AddDirectionalLight({
      .direction = glm::normalize(glm::vec3(0.4f, -1.0f, 0.3f)),
      .color = glm::vec3(1.0f),
      .intensity = 4.0f,
      .castsShadow = true,
  });
}

} // namespace Rodan
