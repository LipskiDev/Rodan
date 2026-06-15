#pragma once

#include "rhi/rhi_handles.h"
#include "rhi/rhi_types.h"
#include <functional>
#include <rhi/rhi_command_list.h>

namespace Rodan {

struct FrameRenderContext {
  Velos::RHI::ImageHandle backbufferImage;
  Velos::RHI::ImageViewHandle backbufferView;
  Velos::RHI::ImageLayout backbufferLayout = Velos::RHI::ImageLayout::Undefined;

  Velos::RHI::ImageHandle depthImage;
  Velos::RHI::ImageViewHandle depthView;

  Velos::RHI::Extent2D extent;

  std::function<void(Velos::RHI::ICommandList &)> renderUi;
};

} // namespace Rodan
