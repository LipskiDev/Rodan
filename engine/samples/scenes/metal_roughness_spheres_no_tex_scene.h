#pragma once

#include "assets/gltf_loader.h"
#include "core/input_system.h"
#include "renderer/mesh_renderer.h"
#include "samples/scene.h"
#include "scene/first_person_camera.h"
#include "scene/static_mesh_instance.h"

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
  void Render(Velos::RHI::ICommandList &cmd) override;
  void RenderImGui() override;

private:
  struct PushConstants {
    glm::mat4 model;
    glm::mat4 view;
    glm::mat4 proj;
  };

  void CreatePipeline(Velos::RHI::IDevice *device);
  void LoadScene(Velos::RHI::IDevice *device);
  void SpawnNodeRecursive(int nodeIndex, const glm::mat4 &parentTransform);

private:
  Velos::RHI::IDevice *device_ = nullptr;
  Velos::RHI::SwapchainHandle swapchain_{};
  Velos::RHI::Format colorFormat_{};
  Velos::RHI::Format depthFormat_{};

  Velos::RHI::ShaderHandle vertexShader_{};
  Velos::RHI::ShaderHandle fragmentShader_{};
  Velos::RHI::PipelineHandle pipeline_{};

  ImportedScene importedScene_;
  std::vector<std::shared_ptr<MeshResource>> uploadedMeshes_;
  std::vector<StaticMeshInstance> instances_;

  MeshRenderer meshRenderer_;
  FirstPersonCamera camera_;

  bool firstMouse_ = true;
  float lastMouseX_ = 0.0f;
  float lastMouseY_ = 0.0f;

  Velos::u32 drawInstanceCount_ = 0;
  Velos::u32 drawSubmeshCount_ = 0;
};

} // namespace Rodan
