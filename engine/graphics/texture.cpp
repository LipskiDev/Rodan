#include "rhi/types.h"
#include <graphics/texture.h>
#include <stdexcept>

namespace Rodan {

Texture CreateTexture2D(Velos::RHI::IDevice *device,
                        Velos::RHI::IUploadContext *upload,
                        const TextureDesc &desc, const void *pixels,
                        uint64_t size) {
  if (!device) {
    throw std::runtime_error("CreateTexture2D: device is null");
  }

  if (!upload) {
    throw std::runtime_error("CreateTexture2D: upload is null");
  }

  if (desc.width == 0 || desc.height == 0) {
    throw std::runtime_error(
        "CreateTexture2D: width and height must be greater than 0");
  }

  if (desc.format == Velos::RHI::Format::Undefined) {
    throw std::runtime_error("CreateTexture2D: format must not be Undefined");
  }

  if (!pixels) {
    throw std::runtime_error("CreateTexture2D: pixels is null");
  }

  if (size == 0) {
    throw std::runtime_error("CreateTexture2D: size must be greater than 0");
  }

  Texture texture{};
  texture.width = desc.width;
  texture.height = desc.height;
  texture.format = desc.format;

  texture.image = device->CreateImage({
      .width = desc.width,
      .height = desc.height,
      .depth = 1,
      .mipLevels = 1,
      .arrayLayers = 1,
      .format = desc.format,
      .type = ImageType::Image2D,
      .usage = ImageUsage::TransferDst | ImageUsage::Sampled,
      .debugName = desc.debugName,
  });

  texture.view = device->CreateImageView({
      .image = texture.image,
      .format = desc.format,
      .type = Velos::RHI::ImageViewType::View2D,
      .baseMipLevel = 0,
      .mipLevelCount = 1,
      .baseArrayLayer = 0,
      .arrayLayerCount = 1,
      .debugName = desc.debugName,
  });

  texture.sampler = device->CreateSampler({
      .minFilter = desc.minFilter,
      .magFilter = desc.magFilter,
      .addressU = desc.addressU,
      .addressV = desc.addressV,
      .addressW = desc.addressW,
      .debugName = desc.debugName,
  });

  upload->UploadImage(
      {
          .dstImage = texture.image,
          .oldLayout = ImageLayout::Undefined,
          .finalLayout = ImageLayout::ShaderReadOnly,
          .mipLevel = 0,
          .baseArrayLayer = 0,
          .layerCount = 1,
          .width = desc.width,
          .height = desc.height,
          .depth = 1,
          .bufferRowLength = 0,
          .bufferImageHeight = 0,
      },
      pixels, size);

  return texture;
}

Texture CreateSolidColorTexture(Velos::RHI::IDevice *device,
                                Velos::RHI::IUploadContext *upload,
                                const glm::vec4 &color, const char *debugName) {
    return Texture{};
}

void DestroyTexture(Velos::RHI::IDevice *device, Texture &texture) {
  if (!device) {
    return;
  }

  if (texture.sampler.IsValid()) {
    device->DestroySampler(texture.sampler);
    texture.sampler = {};
  }

  if (texture.view.IsValid()) {
    device->DestroyImageView(texture.view);
    texture.view = {};
  }

  if (texture.image.IsValid()) {
    device->DestroyImage(texture.image);
    texture.image = {};
  }

  texture.width = 0;
  texture.height = 0;
  texture.format = Velos::RHI::Format::Undefined;
}
} // namespace Rodan
