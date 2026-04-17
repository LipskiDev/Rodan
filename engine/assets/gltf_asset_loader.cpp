#include <assets/gltf_asset_loader.h>
#include <assets/gltf_loader.h>
#include <graphics/mesh_uploader.h>

namespace Rodan {

std::unique_ptr<StaticGltfAsset>
StaticGltfAsset::Load(IDevice *device, const std::string &path) {
  if (!device) {
    throw std::runtime_error("StaticGltfAsset::Load: device is null");
  }

  auto asset = std::make_unique<StaticGltfAsset>();
  asset->device_ = device;

  asset->importedScene_ = GltfLoader::Load(path);
  asset->CreateMaterialLayout(device);
  asset->CreateDescriptorPool(device);
  asset->CreateFallbackResources(device);
  asset->UploadMeshes(device);
  asset->UploadMaterials(device);
  asset->BuildInstances();

  return asset;
}

void StaticGltfAsset::Destroy(IDevice *device) {
  if (!device) {
    return;
  }

  instances_.clear();
  importedScene_ = {};

  for (auto &mesh : meshes_) {
    if (!mesh) {
      continue;
    }

    if (mesh->vertexBuffer.IsValid()) {
      device->DestroyBuffer(mesh->vertexBuffer);
      mesh->vertexBuffer = {};
    }

    if (mesh->indexBuffer.IsValid()) {
      device->DestroyBuffer(mesh->indexBuffer);
      mesh->indexBuffer = {};
    }
  }
  meshes_.clear();

  for (auto &mat : materials_) {
    // only destroy if these are owned per-material and valid
    if (mat.baseColorSampler.IsValid()) {
      device->DestroySampler(mat.baseColorSampler);
      mat.baseColorSampler = {};
    }

    if (mat.baseColorImageView.IsValid()) {
      device->DestroyImageView(mat.baseColorImageView);
      mat.baseColorImageView = {};
    }

    if (mat.baseColorImage.IsValid()) {
      device->DestroyImage(mat.baseColorImage);
      mat.baseColorImage = {};
    }
  }
  materials_.clear();

  if (fallbackSampler_.IsValid()) {
    device->DestroySampler(fallbackSampler_);
    fallbackSampler_ = {};
  }

  if (fallbackImageView_.IsValid()) {
    device->DestroyImageView(fallbackImageView_);
    fallbackImageView_ = {};
  }

  if (fallbackImage_.IsValid()) {
    device->DestroyImage(fallbackImage_);
    fallbackImage_ = {};
  }

  if (fallbackBuffer_.IsValid()) {
    device->DestroyBuffer(fallbackBuffer_);
    fallbackBuffer_ = {};
  }

  if (descriptorPool_.IsValid()) {
    device->DestroyDescriptorPool(descriptorPool_);
    descriptorPool_ = {};
  }

  if (materialLayout_.IsValid()) {
    device->DestroyDescriptorSetLayout(materialLayout_);
    materialLayout_ = {};
  }

  prepared_ = false;
  device_ = nullptr;
}

void StaticGltfAsset::CreateMaterialLayout(IDevice *device) {

  DescriptorBindingDesc binding{};
  binding.binding = 0;
  binding.type = DescriptorType::CombinedImageSampler;
  binding.count = 1;
  binding.visibility = ShaderStage::Fragment;

  DescriptorSetLayoutDesc layout{};
  layout.bindings = &binding;
  layout.bindingCount = 1;
  layout.debugName = "GLTF Material Layout";

  materialLayout_ = device->CreateDescriptorSetLayout(layout);
}

void StaticGltfAsset::CreateDescriptorPool(IDevice *device) {

  const Velos::u32 materialCount = std::max<Velos::u32>(
      1, static_cast<Velos::u32>(importedScene_.materials.size()));

  DescriptorPoolSize poolSize{};
  poolSize.type = DescriptorType::CombinedImageSampler;
  poolSize.count = materialCount;

  DescriptorPoolDesc poolDesc{};
  poolDesc.poolSizes = &poolSize;
  poolDesc.poolSizeCount = 1;
  poolDesc.maxSets = materialCount;

  descriptorPool_ = device->CreateDescriptorPool(poolDesc);
}

void StaticGltfAsset::Prepare(ICommandList &cmd) {
  if (prepared_) {
    return;
  }

  cmd.Barrier({
      .image = fallbackImage_,
      .newLayout = ImageLayout::TransferDst,
      .aspect = ImageAspect::Color,
  });

  BufferImageCopyRegion fallbackRegion{};
  fallbackRegion.bufferOffset = 0;
  fallbackRegion.bufferRowLength = 0;
  fallbackRegion.bufferImageHeight = 0;
  fallbackRegion.mipLevel = 0;
  fallbackRegion.baseArrayLayer = 0;
  fallbackRegion.layerCount = 1;
  fallbackRegion.imageOffset = {0, 0, 0};
  fallbackRegion.imageExtent = {1, 1, 1};
  fallbackRegion.aspect = ImageAspect::Color;

  cmd.CopyBufferToImage(fallbackBuffer_, fallbackImage_, fallbackRegion);

  cmd.Barrier({
      .image = fallbackImage_,
      .newLayout = ImageLayout::ShaderReadOnly,
      .aspect = ImageAspect::Color,
  });

  for (auto &mat : materials_) {
    if (!mat.ownsBaseColorResources || mat.uploaded) {
      continue;
    }

    cmd.Barrier({
        .image = mat.baseColorImage,
        .newLayout = ImageLayout::TransferDst,
        .aspect = ImageAspect::Color,
    });

    BufferImageCopyRegion region{};
    region.bufferOffset = 0;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;
    region.mipLevel = 0;
    region.baseArrayLayer = 0;
    region.layerCount = 1;
    region.imageOffset = {0, 0, 0};
    region.imageExtent = {mat.baseColorWidth, mat.baseColorHeight, 1};
    region.aspect = ImageAspect::Color;

    cmd.CopyBufferToImage(mat.baseColorStagingBuffer, mat.baseColorImage,
                          region);

    cmd.Barrier({
        .image = mat.baseColorImage,
        .newLayout = ImageLayout::ShaderReadOnly,
        .aspect = ImageAspect::Color,
    });

    mat.uploaded = true;
  }

  prepared_ = true;
}

void StaticGltfAsset::UploadMeshes(IDevice *device) {

  meshes_.reserve(importedScene_.meshes.size());
  for (const auto &mesh : importedScene_.meshes) {

    auto gpuMesh = MeshUploader::Upload(device, mesh);

    meshes_.push_back(gpuMesh);
  }
}

void StaticGltfAsset::UploadMaterials(IDevice *device) {
  materials_.clear();
  materials_.reserve(importedScene_.materials.size());

  for (const auto &mat : importedScene_.materials) {
    MaterialResource gpuMat{};

    gpuMat.baseColorFactor = mat.baseColorFactor;
    gpuMat.metallicFactor = mat.metallicFactor;
    gpuMat.roughnessFactor = mat.roughnessFactor;

    if (mat.baseColorTexture.imageIndex >= 0 &&
        mat.baseColorTexture.imageIndex <
            static_cast<int>(importedScene_.images.size())) {
      const ImportedImage &img =
          importedScene_.images[mat.baseColorTexture.imageIndex];

      gpuMat.baseColorWidth = static_cast<uint32_t>(img.width);
      gpuMat.baseColorHeight = static_cast<uint32_t>(img.height);
      gpuMat.ownsBaseColorResources = true;

      gpuMat.baseColorStagingBuffer = device->CreateBuffer({
          .size = static_cast<uint64_t>(img.pixelsRGBA8.size()),
          .usage = BufferUsage::TransferSrc,
          .memoryUsage = MemoryUsage::CPUToGPU,
          .initialData = img.pixelsRGBA8.data(),
          .debugName = "GLTF Base Color Staging Buffer",
      });

      gpuMat.baseColorImage = device->CreateImage({
          .width = gpuMat.baseColorWidth,
          .height = gpuMat.baseColorHeight,
          .depth = 1,
          .mipLevels = 1,
          .arrayLayers = 1,
          .format = Format::RGBA8_UNORM, // use SRGB later if available
          .type = ImageType::Image2D,
          .usage = ImageUsage::TransferDst | ImageUsage::Sampled,
          .debugName = "GLTF Base Color Image",
      });

      gpuMat.baseColorImageView = device->CreateImageView({
          .image = gpuMat.baseColorImage,
          .format = Format::RGBA8_UNORM,
          .type = ImageViewType::View2D,
          .aspect = ImageAspect::Color,
          .baseMipLevel = 0,
          .mipLevelCount = 1,
          .baseArrayLayer = 0,
          .arrayLayerCount = 1,
          .debugName = "GLTF Base Color Image View",
      });

      gpuMat.baseColorSampler = device->CreateSampler({
          .minFilter = Filter::Linear,
          .magFilter = Filter::Linear,
          .addressU = SamplerAddressMode::Repeat,
          .addressV = SamplerAddressMode::Repeat,
          .addressW = SamplerAddressMode::Repeat,
          .debugName = "GLTF Base Color Sampler",
      });
    } else {
      gpuMat.baseColorImage = fallbackImage_;
      gpuMat.baseColorImageView = fallbackImageView_;
      gpuMat.baseColorSampler = fallbackSampler_;
      gpuMat.baseColorWidth = 1;
      gpuMat.baseColorHeight = 1;
      gpuMat.ownsBaseColorResources = false;
    }

    gpuMat.descriptorSet =
        device->AllocateDescriptorSet(descriptorPool_, materialLayout_);

    DescriptorImageInfo imageInfo{};
    imageInfo.sampler = gpuMat.baseColorSampler;
    imageInfo.imageView = gpuMat.baseColorImageView;
    imageInfo.imageLayout = ImageLayout::ShaderReadOnly;

    device->UpdateDescriptorSet({
        .dstSet = gpuMat.descriptorSet,
        .binding = 0,
        .arrayElement = 0,
        .type = DescriptorType::CombinedImageSampler,
        .bufferInfo = nullptr,
        .imageInfo = &imageInfo,
        .descriptorCount = 1,
    });

    materials_.push_back(gpuMat);
  }
}

void StaticGltfAsset::BuildInstances() {

  std::function<void(int, const glm::mat4 &)> spawn;
  spawn = [&](int nodeIndex, const glm::mat4 &parent) {
    const auto &node = importedScene_.nodes[nodeIndex];

    glm::mat4 world = parent * node.transform;

    if (node.meshIndex >= 0) {
      if (node.meshIndex >= static_cast<int>(meshes_.size())) {
        throw std::runtime_error("StaticGltfAsset: invalid mesh index in node");
      }

      StaticMeshInstance instance{};
      instance.mesh = meshes_[node.meshIndex];
      instance.transform = world;

      instances_.push_back(instance);
    }

    for (int child : node.children) {
      spawn(child, world);
    }
  };

  for (int root : importedScene_.rootNodes) {
    spawn(root, glm::mat4(1.0f));
  }

  if (instances_.empty()) {
    throw std::runtime_error(
        "StaticGltfAsset: no instances generated from scene");
  }
}

void StaticGltfAsset::CreateFallbackResources(IDevice *device) {
  if (!device) {
    throw std::runtime_error(
        "StaticGltfAsset::CreateFallbackResources: device is null");
  }

  if (fallbackImage_.IsValid()) {
    return;
  }

  const std::uint8_t whitePixel[4] = {255, 255, 255, 255};

  fallbackBuffer_ = device->CreateBuffer({
      .size = 4,
      .usage = BufferUsage::TransferSrc,
      .memoryUsage = MemoryUsage::CPUToGPU,
      .initialData = whitePixel,
      .debugName = "Fallback White Buffer",
  });

  fallbackImage_ = device->CreateImage({
      .width = 1,
      .height = 1,
      .depth = 1,
      .mipLevels = 1,
      .arrayLayers = 1,
      .format = Format::RGBA8_UNORM,
      .type = ImageType::Image2D,
      .usage = ImageUsage::TransferDst | ImageUsage::Sampled,
      .debugName = "Fallback White Texture",
  });

  fallbackImageView_ = device->CreateImageView({
      .image = fallbackImage_,
      .format = Format::RGBA8_UNORM,
      .type = ImageViewType::View2D,
      .aspect = ImageAspect::Color,
      .baseMipLevel = 0,
      .mipLevelCount = 1,
      .baseArrayLayer = 0,
      .arrayLayerCount = 1,
  });

  fallbackSampler_ = device->CreateSampler({
      .minFilter = Filter::Linear,
      .magFilter = Filter::Linear,
      .addressU = SamplerAddressMode::Repeat,
      .addressV = SamplerAddressMode::Repeat,
      .addressW = SamplerAddressMode::Repeat,
      .debugName = "Fallback White Sampler",
  });
}

void StaticGltfAsset::UploadFallbackIfNeeded(ICommandList &cmd) {
  if (fallbackUploaded_)
    return;

  cmd.Barrier({
      .image = fallbackImage_,
      .newLayout = ImageLayout::TransferDst,
      .aspect = ImageAspect::Color,
  });

  BufferImageCopyRegion region{};
  region.bufferOffset = 0;
  region.mipLevel = 0;
  region.baseArrayLayer = 0;
  region.layerCount = 1;
  region.imageOffset = {0, 0, 0};
  region.imageExtent = {1, 1, 1};
  region.aspect = ImageAspect::Color;

  cmd.CopyBufferToImage(fallbackBuffer_, fallbackImage_, region);

  cmd.Barrier({
      .image = fallbackImage_,
      .newLayout = ImageLayout::ShaderReadOnly,
      .aspect = ImageAspect::Color,
  });

  fallbackUploaded_ = true;
}
} // namespace Rodan
