#pragma once

#include "samples/scene.h"
#include "scene/first_person_camera.h"

#include <glm/glm.hpp>
#include <vector>

namespace Rodan {

class DuckScene final : public IScene {
public:
  DuckScene() = default;
  ~DuckScene() override = default;

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
  using u32 = Velos::u32;
  using u64 = Velos::u64;

  struct DuckVertex {
    glm::vec3 position;
    glm::vec2 uv;
  };

  struct DuckPushConstants {
    glm::mat4 model;
    glm::mat4 view;
    glm::mat4 proj;
  };

  struct DuckMeshData {
    std::vector<DuckVertex> vertices;
    std::vector<u32> indices;
    std::vector<u32> lodIndices;
  };

  struct Resources {
    Velos::RHI::BufferHandle vertexBuffer{};
    Velos::RHI::BufferHandle indexBuffer{};
    Velos::RHI::BufferHandle lodIndexBuffer{};
    Velos::RHI::BufferHandle stagingBuffer{};

    Velos::RHI::ImageHandle image{};
    Velos::RHI::ImageViewHandle imageView{};
    Velos::RHI::SamplerHandle sampler{};

    Velos::RHI::DescriptorSetLayoutHandle setLayout{};
    Velos::RHI::DescriptorPoolHandle descriptorPool{};
    Velos::RHI::DescriptorSetHandle descriptorSet{};

    Velos::RHI::ShaderHandle vertexShader{};
    Velos::RHI::ShaderHandle fragmentShader{};
    Velos::RHI::PipelineHandle pipeline{};

    u32 vertexCount = 0;
    u32 indexCount = 0;
    u32 lodIndexCount = 0;

    u32 textureWidth = 0;
    u32 textureHeight = 0;

    bool uploaded = false;
  };

  DuckMeshData LoadDuckMesh() const;
  void CreateResources(Velos::RHI::IDevice *device);
  void CreateDescriptors(Velos::RHI::IDevice *device);
  void CreatePipeline(Velos::RHI::IDevice *device);
  void UploadTextureIfNeeded(Velos::RHI::ICommandList &cmd);
  void RenderDuckInstance(Velos::RHI::ICommandList &cmd, const glm::mat4 &model,
                          Velos::RHI::BufferHandle indexBuffer, u32 indexCount);

private:
  Velos::RHI::IDevice *device_ = nullptr;
  Velos::RHI::SwapchainHandle swapchain_{};
  Velos::RHI::Format colorFormat_ = Velos::RHI::Format::Undefined;
  Velos::RHI::Format depthFormat_ = Velos::RHI::Format::Undefined;

  Resources duck_{};

  glm::mat4 leftDuckModel_ = glm::mat4(1.0f);
  glm::mat4 rightDuckModel_ = glm::mat4(1.0f);

  float lodRatio_ = 0.2f;

private:
  FirstPersonCamera camera_;

  bool firstMouse_ = true;
  float lastMouseX_ = 0.0f;
  float lastMouseY_ = 0.0f;
};

} // namespace Rodan
