#pragma once

#include "rhi/rhi_handles.h"
#include "samples/scene.h"
#include "scene/first_person_camera.h"

#include <glm/glm.hpp>
#include <vector>

namespace Rodan {

class MillionCubesScene final : public IScene {
public:
  MillionCubesScene() = default;
  ~MillionCubesScene() override = default;

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

  struct MillionCubesVertex {
    glm::vec3 position;
  };

  struct MillionCubesPushConstants {
    glm::mat4 viewproj;
    uint32_t bufIdLo;
    uint32_t bufIdHi;
  };

  struct Resources {
    uint32_t textureWidth, textureHeight;

    Velos::RHI::ImageHandle xorPatternImage;
    Velos::RHI::ImageViewHandle xorPatternImageView;
    Velos::RHI::SamplerHandle xorPatternSampler;

    Velos::RHI::BufferHandle positionBuffer;
    Velos::RHI::BufferHandle stagingBuffer;

    Velos::RHI::DescriptorSetLayoutHandle setLayout{};
    Velos::RHI::DescriptorPoolHandle descriptorPool{};
    Velos::RHI::DescriptorSetHandle descriptorSet{};

    Velos::RHI::ShaderHandle vertexShader{};
    Velos::RHI::ShaderHandle fragmentShader{};
    Velos::RHI::PipelineHandle pipeline{};

    bool uploaded = false;
  };

  void CreateResources(Velos::RHI::IDevice *device);
  void CreateDescriptors(Velos::RHI::IDevice *device);
  void CreatePipeline(Velos::RHI::IDevice *device);
  void UploadTextureIfNeeded(Velos::RHI::ICommandList &cmd);
  void RenderMillionCubes(Velos::RHI::ICommandList &cmd);

private:
  Velos::RHI::IDevice *device_ = nullptr;
  Velos::RHI::SwapchainHandle swapchain_{};
  Velos::RHI::Format colorFormat_ = Velos::RHI::Format::Undefined;
  Velos::RHI::Format depthFormat_ = Velos::RHI::Format::Undefined;

  Resources cubes_{};

  glm::mat4 millionCubesModel_ = glm::mat4(1.0f);

private:
  FirstPersonCamera camera_;

  bool firstMouse_ = true;
  float lastMouseX_ = 0.0f;
  float lastMouseY_ = 0.0f;

  const uint64_t k_CubesCount = 1 * 1024 * 1024;
};

} // namespace Rodan
