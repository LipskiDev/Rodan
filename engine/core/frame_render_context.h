#pragma once

#include "rhi/handles.h"
#include "rhi/types.h"
#include <functional>
#include <rhi/command_list.h>

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
