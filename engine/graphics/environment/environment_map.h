#pragma once

#include "rhi/rhi_command_list.h"
#include "rhi/rhi_device.h"
#include "rhi/rhi_types.h"

#include <filesystem>
#include <memory>

namespace Rodan {

class EnvironmentMap {
public:
  static std::shared_ptr<EnvironmentMap>
  LoadHDR(Velos::RHI::IDevice *device, const std::filesystem::path &path);

  void CreateFromHDR(Velos::RHI::IDevice *device,
                     const std::filesystem::path &path);

  void RecordUpload(Velos::RHI::ICommandList &cmd);
  void Destroy();

  bool NeedsUpload() const { return !uploaded_; }

  Velos::RHI::DescriptorSetLayoutHandle GetDescriptorSetLayout() const {
    return setLayout_;
  }

  Velos::RHI::DescriptorSetHandle GetDescriptorSet() const {
    return descriptorSet_;
  }

private:
  void CreateDescriptorResources();

private:
  Velos::RHI::IDevice *device_ = nullptr;

  Velos::RHI::BufferHandle stagingBuffer_{};
  Velos::RHI::ImageHandle image_{};
  Velos::RHI::ImageViewHandle view_{};
  Velos::RHI::SamplerHandle sampler_{};

  Velos::RHI::DescriptorSetLayoutHandle setLayout_{};
  Velos::RHI::DescriptorPoolHandle descriptorPool_{};
  Velos::RHI::DescriptorSetHandle descriptorSet_{};

  Velos::u32 faceSize_ = 0;
  Velos::u64 cubemapByteSize_ = 0;
  uint32_t mipLevels_;

  bool uploaded_ = false;
};

} // namespace Rodan
