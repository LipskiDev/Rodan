#pragma once

#include "glm/ext/vector_float4.hpp"
#include "rhi/rhi_device.h"
#include "rhi/rhi_handles.h"
#include "rhi/rhi_types.h"
namespace Rodan {

using namespace Velos::RHI;

struct TextureDesc {
  uint32_t width = 0;
  uint32_t height = 0;
  Format format = Format::Undefined;

  Filter minFilter = Filter::Linear;
  Filter magFilter = Filter::Linear;

  SamplerAddressMode addressU = SamplerAddressMode::Repeat;
  SamplerAddressMode addressV = SamplerAddressMode::Repeat;
  SamplerAddressMode addressW = SamplerAddressMode::Repeat;

  bool generateMipmaps = false;
  const char *debugName = nullptr;
};

struct TextureCreateData {
  const void *pixels = nullptr;
  uint64_t size = 0;
};

struct Texture {
  Velos::RHI::ImageHandle image;
  Velos::RHI::ImageViewHandle view;
  Velos::RHI::SamplerHandle sampler;

  uint32_t width = 0;
  uint32_t height = 0;
  Velos::RHI::Format format = Velos::RHI::Format::Undefined;

  bool IsValid() const {
    return image.IsValid() && view.IsValid() && sampler.IsValid();
  }
};

Texture CreateTexture2D(Velos::RHI::IDevice *device,
                        Velos::RHI::IUploadContext *upload,
                        const TextureDesc &desc, const void *pixels,
                        uint64_t size);

Texture CreateSolidColorTexture(Velos::RHI::IDevice *device,
                                Velos::RHI::IUploadContext *upload,
                                const glm::vec4 &color, const char *debugName);

void DestroyTexture(Velos::RHI::IDevice *device, Texture &texture);
} // namespace Rodan
