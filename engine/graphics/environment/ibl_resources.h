#pragma once

#include "graphics/texture.h"
#include "rhi/rhi_device.h"
#include "rhi/rhi_handles.h"

#include <array>

namespace Rodan {

struct IBLResources {
  Texture irradianceTexture{};

  std::array<Velos::RHI::ImageViewHandle, 6> irradianceFaceViews{};

  Velos::RHI::DescriptorSetLayoutHandle descriptorSetLayout{};
  Velos::RHI::DescriptorPoolHandle descriptorPool{};
  Velos::RHI::DescriptorSetHandle descriptorSet{};

  void Destroy(Velos::RHI::IDevice *device);
};

} // namespace Rodan
