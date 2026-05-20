#pragma once

#include "graphics/texture.h"
#include "rhi/rhi_device.h"
#include "rhi/rhi_handles.h"

#include <array>

namespace Rodan {

struct IBLResources {
  Texture irradianceTexture{};
  Texture prefilterTexture{};
  Texture brdfLutTexture{};

  std::array<ImageViewHandle, 6> irradianceFaceViews;
  std::array<Velos::RHI::ImageViewHandle, 6 * 8> prefilterFaceMipViews{};
  ImageViewHandle brdfLutView{};

  Velos::RHI::DescriptorSetLayoutHandle descriptorSetLayout{};
  Velos::RHI::DescriptorPoolHandle descriptorPool{};
  Velos::RHI::DescriptorSetHandle descriptorSet{};

  void Destroy(Velos::RHI::IDevice *device);
};

} // namespace Rodan
