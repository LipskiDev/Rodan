#pragma once

#include "rhi/rhi_handles.h"
#include "rhi/rhi_types.h"
#include "samples/scene.h"

namespace Rodan {

class ComputeTestScene : public IScene {
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
  void CreateOutputImage(Velos::u32 width, Velos::u32 height);
  void DestroyOutputImage();

private:
  Velos::RHI::IDevice *device_ = nullptr;
  Velos::RHI::SwapchainHandle swapchain_{};

  Velos::u32 width_ = 512;
  Velos::u32 height_ = 512;

  Velos::RHI::ShaderHandle computeShader_{};
  Velos::RHI::PipelineHandle computePipeline_{};

  Velos::RHI::DescriptorSetLayoutHandle descriptorSetLayout_{};
  Velos::RHI::DescriptorPoolHandle descriptorPool_{};
  Velos::RHI::DescriptorSetHandle descriptorSet_{};

  Velos::RHI::ImageHandle outputImage_{};
  Velos::RHI::ImageViewHandle outputImageView_{};

  bool dispatched_ = false;

  Velos::RHI::SamplerHandle outputSampler_{};

  Velos::RHI::DescriptorSetLayoutHandle displaySetLayout_{};
  Velos::RHI::DescriptorPoolHandle displayDescriptorPool_{};
  Velos::RHI::DescriptorSetHandle displayDescriptorSet_{};

  Velos::RHI::ShaderHandle fullscreenVS_{};
  Velos::RHI::ShaderHandle fullscreenFS_{};
  Velos::RHI::PipelineHandle fullscreenPipeline_{};

  Velos::RHI::BufferHandle patternBuffer_{};
  static constexpr Velos::u32 kPatternColorCount = 256;
};

} // namespace Rodan
