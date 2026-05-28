#include <chrono>
#include <iostream>
#include <samples/scenes/gltf_viewer_scene.h>

#include <nfd.h>

#include "glm/geometric.hpp"
#include "imgui.h"
#include "imgui_internal.h"
#include "rhi/rhi_types.h"
#include "scene/handles.h"
#include "scene/orbit_camera.h"

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
  colorFormat_ = colorFormat;
  depthFormat_ = depthFormat;

  if (NFD_Init() != NFD_OKAY) {
    std::cerr << "Failed to initialize NFD: " << NFD_GetError() << std::endl;
  }

  SetCameraMode(CameraMode::Orbit);

  LoadScene(device_, currentScenePath_);
  currentBounds_ = ComputeCurrentBounds();
  ComputeStats();
  FrameCamera();
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

  if (pendingSkyboxReload_) {
    pendingSkyboxReload_ = false;
    try {
      device_->WaitIdle();
      sceneRenderer_.LoadEnvironment(device_, pendingSkyboxPath_);
      currentSkyboxPath_ = pendingSkyboxPath_;

      std::cout << "Loaded skybox: " << currentSkyboxPath_ << std::endl;
    } catch (const std::exception &e) {
      std::cerr << "Failed to load skybox: " << e.what() << std::endl;
    }
  }

  if (changed) {
    for (RenderObjectHandle handle : currentRenderTargets_) {
      renderWorld_.SetTransform(handle, currentTransform_);
    }
    changed = false;
  }

  camera_->SetPerspective(60.0f,
                          static_cast<float>(ctx.framebufferWidth) /
                              static_cast<float>(ctx.framebufferHeight),
                          0.1f, 500.0f);

  if (ctx.input) {
    for (const InputEvent &event : ctx.input->GetEvents()) {
      if (event.type == InputEventType::KeyDown ||
          event.type == InputEventType::KeyUp) {
        camera_->OnKeyboard(event);
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
          camera_->OnMouseMove(dx, dy);
        }
      }
    }
  }

  camera_->Update(deltaSeconds);
}

void GltfViewerScene::Prepare(ICommandList &cmd) { (void)cmd; }

void GltfViewerScene::Render(ICommandList &cmd,
                             const FrameRenderContext &frame) {
  sceneRenderer_.Render(cmd, renderWorld_, *camera_, frame, dbgCtx_);
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

  ImGui::SeparatorText("Environment");

  ImGui::Text("Skybox:");
  ImGui::TextWrapped("%s", currentSkyboxPath_.c_str());

  if (ImGui::Button("Open HDR / Skybox")) {
    nfdu8char_t *outPath = nullptr;

    nfdu8filteritem_t filters[] = {
        {"Environment maps", "hdr,exr,ktx,ktx2,png,jpg,jpeg"},
    };

    nfdopendialogu8args_t args = {};
    args.filterList = filters;
    args.filterCount = 1;

    nfdresult_t result = NFD_OpenDialogU8_With(&outPath, &args);

    if (result == NFD_OKAY) {
      pendingSkyboxPath_ = outPath;
      NFD_FreePathU8(outPath);
      pendingSkyboxReload_ = true;
    } else if (result != NFD_CANCEL) {
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

  ImGui::Checkbox("Auto Scale Imported Models", &autoScaleModel_);
  ImGui::Checkbox("Auto Center Imported Models", &autoCenterModel_);

  if (ImGui::Button("Reset Transform")) {
    currentTransform_ = Rodan::Transform{};
    changed = true;
  }

  ImGui::Separator();
  ImGui::Text("Camera");

  const char *cameraModes[] = {
      "First Person",
      "Orbit",
  };

  int currentMode = static_cast<int>(cameraMode_);

  if (ImGui::Combo("Camera Mode", &currentMode, cameraModes,
                   IM_ARRAYSIZE(cameraModes))) {
    SetCameraMode(static_cast<CameraMode>(currentMode));
  }

  if (ImGui::Button("Reset Camera")) {
    camera_->Reset();
  }

  ImGui::SameLine();

  if (ImGui::Button("Frame Camera")) {
    FrameCamera();
  }

  ImGui::Separator();

  const char *items[] = {"Final",   "BaseColor", "Normal", "MetallicRoughness",
                         "Tangent", "Occlusion"};
  int mode = static_cast<int>(dbgCtx_.mode);
  if (ImGui::Combo("Show", &mode, items, IM_ARRAYSIZE(items))) {
    dbgCtx_.mode = static_cast<DrawMode>(mode);
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

  ImGui::Begin("glTF Stats");

  ImGui::Text("Mesh count: %u", stats_.meshCount);
  ImGui::Text("Submesh count: %u", stats_.submeshCount);
  ImGui::Text("Instance count: %u", stats_.instanceCount);
  ImGui::Text("Vertex count: %u", stats_.vertexCount);
  ImGui::Text("Index count: %u", stats_.indexCount);
  ImGui::Text("Triangle count: %u", stats_.triangleCount);
  ImGui::Text("Material count: %u", stats_.materialCount);
  ImGui::Text("Texture count: %u", stats_.textureCount);

  ImGui::SeparatorText("Bounding Boxes");

  ImGui::Checkbox("Scene", &dbgCtx_.drawSceneBounds);
  ImGui::Checkbox("Submeshes", &dbgCtx_.drawMeshBounds);
  ImGui::Checkbox("Light direction", &dbgCtx_.drawLightDirection);

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

  int materialIndex = 0;
  for (const MaterialResource &material : asset_->GetMaterials()) {
    MaterialHandle handle = renderWorld_.AddMaterial(material);
    materialHandles.push_back(handle);

    std::cout << "[Material " << materialIndex << "] "
              << "handle=" << handle.id << " baseColor=("
              << material.baseColorFactor.r << ", "
              << material.baseColorFactor.g << ", "
              << material.baseColorFactor.b << ", "
              << material.baseColorFactor.a << ")"
              << " metallic=" << material.metallicFactor
              << " roughness=" << material.roughnessFactor
              << " alphaMode=" << static_cast<int>(material.alphaMode)
              << " transmission=" << material.transmission.transmissionFactor
              << "\n";

    materialIndex++;
  }

  for (const StaticMeshInstance &instance : instances_) {
    if (!instance.mesh) {
      continue;
    }

    MeshHandle meshHandle = renderWorld_.AddMesh(*instance.mesh);

    std::cout << "\n[Mesh instance]\n";

    for (size_t i = 0; i < instance.mesh->submeshes.size(); ++i) {
      const Submesh &submesh = instance.mesh->submeshes[i];

      std::cout << "  submesh=" << i
                << " materialSlot=" << submesh.materialSlot;

      if (submesh.materialSlot >= 0 &&
          submesh.materialSlot < static_cast<int>(materialHandles.size())) {
        const MaterialResource &mat =
            renderWorld_.GetMaterial(materialHandles[submesh.materialSlot]);

        std::cout << " -> materialHandle="
                  << materialHandles[submesh.materialSlot].id << " baseColor=("
                  << mat.baseColorFactor.r << ", " << mat.baseColorFactor.g
                  << ", " << mat.baseColorFactor.b << ", "
                  << mat.baseColorFactor.a << ")"
                  << " alphaMode=" << static_cast<int>(mat.alphaMode)
                  << " transmission=" << mat.transmission.transmissionFactor;
      } else {
        std::cout << " -> INVALID MATERIAL SLOT";
      }

      std::cout << "\n";
    }

    RenderObjectDesc desc{};
    desc.mesh = meshHandle;
    desc.materials = materialHandles;
    desc.visible = true;
    desc.transform = instance.localTransform;

    currentRenderTargets_.push_back(renderWorld_.CreateObject(desc));
  }
}

void GltfViewerScene::ReloadScene(const std::string &path) {
  if (!device_) {
    return;
  }

  device_->WaitIdle();

  sceneRenderer_.Shutdown(device_);

  renderWorld_.Clear();
  currentRenderTargets_.clear();
  instances_.clear();

  if (asset_) {
    asset_->Destroy(device_);
    asset_.reset();
  }

  LoadScene(device_, path);

  sceneRenderer_.Initialize(device_, swapchain_, colorFormat_, depthFormat_,
                            asset_->GetMaterialLayout());

  sceneRenderer_.LoadEnvironment(device_, currentSkyboxPath_);

  currentBounds_ = ComputeCurrentBounds();

  const float scale = autoScaleModel_ ? RescaleScene(5.0f) : 1.0f;
  const glm::vec3 center = currentBounds_.Center();

  currentTransform_.scale = glm::vec3(scale);
  currentTransform_.position =
      autoCenterModel_ ? -center * scale : glm::vec3(0.0f);

  for (RenderObjectHandle handle : currentRenderTargets_) {
    renderWorld_.SetTransform(handle, currentTransform_);
  }

  changed = false;

  FrameCamera();

  sunLight_ = renderWorld_.AddDirectionalLight({
      .direction = glm::normalize(glm::vec3(0.4f, -1.0f, 0.3f)),
      .color = glm::vec3(1.0f),
      .intensity = 4.0f,
      .castsShadow = true,
  });

  ComputeStats();
}

float GltfViewerScene::RescaleScene(float targetSize) {
  AABB aabb = currentBounds_;
  const glm::vec3 size = aabb.upper - aabb.lower;
  const float maxExtent = std::max(size.x, std::max(size.y, size.z));

  if (maxExtent <= 0.0000001f) {
    return 1.0f;
  }

  return targetSize / maxExtent;
}

void GltfViewerScene::FrameCamera() {
  AABB bounds = ComputeCurrentBounds();

  glm::vec3 center = bounds.Center();
  glm::vec3 size = bounds.upper - bounds.lower;

  float radius = glm::length(size) * 0.5f;
  float fovY = glm::radians(60.0f);

  float distance = radius / std::tan(fovY * 0.5f);
  distance *= 1.5f;

  camera_->SetPosition(center + glm::vec3(0.0f, 0.0f, distance));
  camera_->LookAt(center);
}

AABB GltfViewerScene::ComputeCurrentBounds() {
  AABB aabb;

  for (const auto &obj : renderWorld_.GetObjects()) {
    const MeshResource &mesh = renderWorld_.GetMesh(obj.mesh);

    glm::mat4 model =
        obj.worldTransform.ToMatrix() * obj.localTransform.ToMatrix();

    AABB worldAABB = mesh.aabb.Transform(model);

    aabb.Expand(worldAABB.lower);
    aabb.Expand(worldAABB.upper);
  }

  return aabb;
}

void GltfViewerScene::SetCameraMode(CameraMode mode) {
  cameraMode_ = mode;

  switch (cameraMode_) {
  case CameraMode::FirstPerson:
    camera_ = std::make_unique<FirstPersonCamera>();
    break;

  case CameraMode::Orbit:
    camera_ = std::make_unique<OrbitCamera>();
    break;
  }

  camera_->SetPerspective(60.0f, 16.0f / 9.0f, 0.1f, 500.0f);

  FrameCamera();

  firstMouse_ = true;
}
void GltfViewerScene::ComputeStats() {
  stats_ = {};

  if (!asset_) {
    return;
  }

  const auto &assetStats = asset_->GetStats();

  stats_.meshCount = assetStats.meshCount;
  stats_.submeshCount = assetStats.submeshCount;
  stats_.instanceCount = assetStats.instanceCount;
  stats_.vertexCount = assetStats.vertexCount;
  stats_.indexCount = assetStats.indexCount;
  stats_.triangleCount = assetStats.triangleCount;
  stats_.materialCount = assetStats.materialCount;
  stats_.textureCount = assetStats.textureCount;
}

} // namespace Rodan
