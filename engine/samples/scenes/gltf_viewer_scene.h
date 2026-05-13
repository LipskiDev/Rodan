
#pragma once

#include "rhi/rhi_handles.h"
#include "samples/scene.h"
#include "scene/first_person_camera.h"
#include "scene/handles.h"
#include "scene/orbit_camera.h"
#include "scene/render_world.h"
#include "scene/static_mesh_instance.h"
#include <assets/gltf_asset_loader.h>
#include <renderer/scene_renderer.h>

#include <glm/glm.hpp>
#include <vector>

namespace Rodan {

static bool IsGltfPath(const std::string &path) {
  std::filesystem::path p(path);
  std::string ext = p.extension().string();

  std::transform(ext.begin(), ext.end(), ext.begin(),
                 [](unsigned char c) { return std::tolower(c); });

  return ext == ".gltf" || ext == ".glb";
}

class GltfViewerScene : public IScene {
private:
  enum class CameraMode {
    FirstPerson = 0,
    Orbit = 1,
  };

  struct SceneStats {
    uint32_t meshCount = 0;
    uint32_t submeshCount = 0;
    uint32_t vertexCount = 0;
    uint32_t indexCount = 0;
    uint32_t instanceCount = 0;
    uint32_t triangleCount = 0;
    uint32_t materialCount = 0;
    uint32_t textureCount = 0;
  };

public:
  void Initialize(Velos::RHI::IDevice *device,
                  Velos::RHI::SwapchainHandle swapchain,
                  Velos::RHI::Format colorFormat,
                  Velos::RHI::Format depthFormat) override;

  void Shutdown(Velos::RHI::IDevice *device) override;
  void OnResize(Velos::RHI::IDevice *device, Velos::u32 width,
                Velos::u32 height) override;
  void Update(float deltaSeconds, const SceneUpdateContext &ctx) override;
  void Prepare(Velos::RHI::ICommandList &cmd) override;
  void Render(Velos::RHI::ICommandList &cmd,
              const FrameRenderContext &frame) override;
  void RenderImGui() override;

private:
  void LoadScene(Velos::RHI::IDevice *device, std::string path);
  void ReloadScene(const std::string &path);
  float RescaleScene(float targetSize);
  glm::vec3 CenterScene();
  void FrameCamera();
  AABB ComputeCurrentBounds();
  void SetCameraMode(CameraMode mode);
  void ComputeStats();

private:
  Velos::RHI::IDevice *device_ = nullptr;
  Velos::RHI::SwapchainHandle swapchain_{};

  ImportedScene importedScene_;
  std::vector<std::shared_ptr<MeshResource>> uploadedMeshes_;
  std::vector<StaticMeshInstance> instances_;

  SceneRenderer sceneRenderer_;
  RenderWorld renderWorld_;

  CameraMode cameraMode_ = CameraMode::Orbit;

  std::unique_ptr<Camera> camera_;

  bool firstMouse_ = true;
  float lastMouseX_ = 0.0f;
  float lastMouseY_ = 0.0f;

  Velos::u32 drawInstanceCount_ = 0;
  Velos::u32 drawSubmeshCount_ = 0;

  std::unique_ptr<StaticGltfAsset> asset_;

  DrawMode drawMode_ = DrawMode::Final;

  DirectionalLightHandle sunLight_;

  bool pendingReload_ = false;
  std::string currentScenePath_ =
      "assets/models/compare_normals/CompareNormal.glb";
  std::string pendingScenePath_;

  bool autoScaleModel_ = true;
  bool autoCenterModel_ = true;

  Transform currentTransform_;
  std::vector<RenderObjectHandle> currentRenderTargets_;
  bool lockUniformScale_ = true;
  bool changed = false;

  AABB currentBounds_ = {};

  SceneStats stats_;

  DebugContext dbgCtx_;
};

} // namespace Rodan
