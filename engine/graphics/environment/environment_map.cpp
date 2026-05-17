#include "graphics/environment/environment_map.h"

#include "graphics/bitmap.h"
#include "path.h"

#include <stb_image.h>

#include <cstdint>
#include <filesystem>
#include <stdexcept>
#include <string>

namespace Rodan {

using namespace Velos;
using namespace Velos::RHI;

std::shared_ptr<EnvironmentMap>
EnvironmentMap::LoadHDR(IDevice *device, const std::filesystem::path &path) {
  auto environment = std::make_shared<EnvironmentMap>();
  environment->CreateFromHDR(device, path);
  return environment;
}

void EnvironmentMap::CreateFromHDR(IDevice *device,
                                   const std::filesystem::path &path) {
  if (!device) {
    throw std::runtime_error("EnvironmentMap::CreateFromHDR: device is null");
  }

  device_ = device;

  // stbi_set_flip_vertically_on_load(1);

  int width = 0;
  int height = 0;
  int componentCount = 0;

  const float *imageData =
      stbi_loadf(path.string().c_str(), &width, &height, &componentCount, 4);

  if (!imageData) {
    throw std::runtime_error("EnvironmentMap::CreateFromHDR: failed to load " +
                             path.string());
  }

  Graphics::Bitmap equirectangular(width, height, 4,
                                   Graphics::BitmapFormat::Float, imageData);

  Graphics::Bitmap verticalCross =
      convertEquirectangularMapToVerticalCross(equirectangular);

  stbi_image_free((void *)imageData);

  Graphics::Bitmap cubemap = convertVerticalCrossToCubeMapFaces(verticalCross);

  if (cubemap.w_ != cubemap.h_) {
    throw std::runtime_error(
        "EnvironmentMap::CreateFromHDR: cubemap faces are not square");
  }

  if (cubemap.comp_ != 4) {
    throw std::runtime_error(
        "EnvironmentMap::CreateFromHDR: expected RGBA cubemap");
  }

  faceSize_ = static_cast<u32>(cubemap.w_);

  const u64 bytesPerPixel =
      static_cast<u64>(cubemap.comp_) * static_cast<u64>(sizeof(float));

  const u64 faceByteSize =
      static_cast<u64>(faceSize_) * static_cast<u64>(faceSize_) * bytesPerPixel;

  cubemapByteSize_ = faceByteSize * 6;

  stagingBuffer_ = device_->CreateBuffer({
      .size = cubemapByteSize_,
      .usage = BufferUsage::TransferSrc,
      .memoryUsage = MemoryUsage::CPUToGPU,
      .initialData = cubemap.data_.data(),
      .debugName = "Environment Cubemap Upload Staging Buffer",
  });

  image_ = device_->CreateImage({
      .width = faceSize_,
      .height = faceSize_,
      .depth = 1,
      .mipLevels = 1,
      .arrayLayers = 6,
      .format = Format::RGBA32_FLOAT,
      .type = ImageType::Cube,
      .usage = ImageUsage::TransferDst | ImageUsage::Sampled,
      .debugName = "Environment Cubemap",
  });

  view_ = device_->CreateImageView({
      .image = image_,
      .format = Format::RGBA32_FLOAT,
      .type = ImageViewType::Cube,
      .aspect = ImageAspect::Color,
      .baseMipLevel = 0,
      .mipLevelCount = 1,
      .baseArrayLayer = 0,
      .arrayLayerCount = 6,
      .debugName = "Environment Cubemap View",
  });

  sampler_ = device_->CreateSampler({
      .minFilter = Filter::Linear,
      .magFilter = Filter::Linear,
      .addressU = SamplerAddressMode::ClampToEdge,
      .addressV = SamplerAddressMode::ClampToEdge,
      .addressW = SamplerAddressMode::ClampToEdge,
      .debugName = "Environment Cubemap Sampler",
  });

  CreateDescriptorResources();

  uploaded_ = false;
}

void EnvironmentMap::CreateDescriptorResources() {
  DescriptorBindingDesc bindings[] = {
      {
          .binding = 0,
          .type = DescriptorType::CombinedImageSampler,
          .count = 1,
          .visibility = ShaderStage::Fragment,
      },
  };

  setLayout_ = device_->CreateDescriptorSetLayout({
      .bindings = bindings,
      .bindingCount = 1,
      .debugName = "Environment Descriptor Set Layout",
  });

  DescriptorPoolSize poolSizes[] = {
      {
          .type = DescriptorType::CombinedImageSampler,
          .count = 1,
      },
  };

  descriptorPool_ = device_->CreateDescriptorPool({
      .poolSizes = poolSizes,
      .poolSizeCount = 1,
      .maxSets = 1,
      .debugName = "Environment Descriptor Pool",
  });

  descriptorSet_ = device_->AllocateDescriptorSet(descriptorPool_, setLayout_,
                                                  "Environment Descriptor Set");

  DescriptorImageInfo imageInfo{};
  imageInfo.sampler = sampler_;
  imageInfo.imageView = view_;
  imageInfo.imageLayout = ImageLayout::ShaderReadOnly;

  device_->UpdateDescriptorSet({
      .dstSet = descriptorSet_,
      .binding = 0,
      .arrayElement = 0,
      .type = DescriptorType::CombinedImageSampler,
      .bufferInfo = nullptr,
      .imageInfo = &imageInfo,
      .descriptorCount = 1,
  });
}

void EnvironmentMap::RecordUpload(ICommandList &cmd) {
  if (uploaded_) {
    return;
  }

  if (!stagingBuffer_ || !image_) {
    throw std::runtime_error(
        "EnvironmentMap::RecordUpload: missing staging buffer or image");
  }

  const u32 bytesPerPixel = 4 * sizeof(float);
  const u64 faceByteSize = static_cast<u64>(faceSize_) *
                           static_cast<u64>(faceSize_) *
                           static_cast<u64>(bytesPerPixel);

  cmd.Barrier({
      .image = image_,
      .newLayout = ImageLayout::TransferDst,
      .aspect = ImageAspect::Color,
      .baseMipLevel = 0,
      .mipLevelCount = 1,
      .baseArrayLayer = 0,
      .layerCount = 6,
  });

  for (u32 face = 0; face < 6; ++face) {
    BufferImageCopyRegion region{};

    region.bufferOffset = static_cast<u64>(face) * faceByteSize;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;

    region.mipLevel = 0;
    region.baseArrayLayer = face;
    region.layerCount = 1;

    region.imageOffset = {0, 0, 0};
    region.imageExtent = {
        faceSize_,
        faceSize_,
        1,
    };

    region.aspect = ImageAspect::Color;

    cmd.CopyBufferToImage(stagingBuffer_, image_, region);
  }

  cmd.Barrier({
      .image = image_,
      .newLayout = ImageLayout::ShaderReadOnly,
      .aspect = ImageAspect::Color,
      .baseMipLevel = 0,
      .mipLevelCount = 1,
      .baseArrayLayer = 0,
      .layerCount = 6,
  });

  uploaded_ = true;
}

void EnvironmentMap::Destroy() {
  if (!device_) {
    return;
  }

  if (descriptorPool_) {
    device_->DestroyDescriptorPool(descriptorPool_);
  }

  if (setLayout_) {
    device_->DestroyDescriptorSetLayout(setLayout_);
  }

  if (sampler_) {
    device_->DestroySampler(sampler_);
  }

  if (view_) {
    device_->DestroyImageView(view_);
  }

  if (image_) {
    device_->DestroyImage(image_);
  }

  if (stagingBuffer_) {
    device_->DestroyBuffer(stagingBuffer_);
  }

  descriptorSet_ = {};
  descriptorPool_ = {};
  setLayout_ = {};

  sampler_ = {};
  view_ = {};
  image_ = {};
  stagingBuffer_ = {};

  faceSize_ = 0;
  cubemapByteSize_ = 0;
  uploaded_ = false;
  device_ = nullptr;
}

} // namespace Rodan
