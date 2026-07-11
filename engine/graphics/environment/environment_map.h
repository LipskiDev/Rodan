#pragma once

#include "rhi/command_list.h"
#include "rhi/device.h"
#include "rhi/types.h"

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

  Velos::RHI::BindingLayoutHandle GetBindingLayout() const {
    return setLayout_;
  }

  Velos::RHI::BindingSetHandle GetBindingSet() const {
    return bindingSet_;
  }

private:
  void CreateDescriptorResources();

private:
  Velos::RHI::IDevice *device_ = nullptr;

  Velos::RHI::BufferHandle stagingBuffer_{};
  Velos::RHI::ImageHandle image_{};
  Velos::RHI::ImageViewHandle view_{};
  Velos::RHI::SamplerHandle sampler_{};

  Velos::RHI::BindingLayoutHandle setLayout_{};
  Velos::RHI::BindingPoolHandle bindingPool_{};
  Velos::RHI::BindingSetHandle bindingSet_{};

  Velos::u32 faceSize_ = 0;
  Velos::u64 cubemapByteSize_ = 0;
  uint32_t mipLevels_;

  bool uploaded_ = false;
};

} // namespace Rodan
