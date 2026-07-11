#pragma once

#include "graphics/texture.h"
#include "rhi/device.h"
#include "rhi/handles.h"

#include <array>

namespace Rodan {

struct IBLResources {
  Texture irradianceTexture{};
  Texture prefilterTexture{};
  Texture brdfLutTexture{};

  std::array<ImageViewHandle, 6> irradianceFaceViews;
  std::array<Velos::RHI::ImageViewHandle, 6 * 8> prefilterFaceMipViews{};
  ImageViewHandle brdfLutView{};

  Velos::RHI::BindingLayoutHandle descriptorSetLayout{};
  Velos::RHI::BindingPoolHandle descriptorPool{};
  Velos::RHI::BindingSetHandle descriptorSet{};

  void Destroy(Velos::RHI::IDevice *device);
};

} // namespace Rodan
