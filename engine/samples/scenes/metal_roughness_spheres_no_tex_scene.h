#pragma once

#include "renderer/mesh_renderer.h"
#include "rhi/rhi_handles.h"
#include "samples/scene.h"
#include "scene/first_person_camera.h"
#include "scene/handles.h"
#include "scene/render_world.h"
#include "scene/static_mesh_instance.h"
#include <assets/gltf_asset_loader.h>
#include <renderer/scene_renderer.h>

#include <glm/glm.hpp>
#include <vector>

namespace Rodan {

class MetalRoughnessSpheresNoTexScene : public IScene {
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
  void LoadScene(Velos::RHI::IDevice *device);

private:
  Velos::RHI::IDevice *device_ = nullptr;
  Velos::RHI::SwapchainHandle swapchain_{};

  ImportedScene importedScene_;
  std::vector<std::shared_ptr<MeshResource>> uploadedMeshes_;
  std::vector<StaticMeshInstance> instances_;

  SceneRenderer sceneRenderer_;
  RenderWorld renderWorld_;
  FirstPersonCamera camera_;

  bool firstMouse_ = true;
  float lastMouseX_ = 0.0f;
  float lastMouseY_ = 0.0f;

  Velos::u32 drawInstanceCount_ = 0;
  Velos::u32 drawSubmeshCount_ = 0;

  std::unique_ptr<StaticGltfAsset> asset_;

  enum Show {
    BaseColor = 0,
    Normal = 1,
    MetallicRoughness = 2,
    Tangent = 3,
    Final = 4
  };
  Show showMode_ = Final;

  DirectionalLightHandle sunLight_;
};

} // namespace Rodan
