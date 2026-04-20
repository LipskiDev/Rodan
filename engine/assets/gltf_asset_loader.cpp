#include "rhi/rhi_types.h"
#include "rhi/rhi_upload_context.h"
#include <assets/gltf_asset_loader.h>
#include <assets/gltf_loader.h>
#include <graphics/mesh_uploader.h>
#include <stdexcept>

namespace Rodan {

std::unique_ptr<StaticGltfAsset>
StaticGltfAsset::Load(IDevice *device, IUploadContext *upload,
                      const std::string &path) {
  if (!device) {
    throw std::runtime_error("StaticGltfAsset::Load: device is null");
  }

  if (!upload) {
    throw std::runtime_error("StaticGltfAsset::Load: upload context is null");
  }

  auto asset = std::make_unique<StaticGltfAsset>();
  asset->device_ = device;

  asset->importedScene_ = GltfLoader::Load(path);
  asset->CreateMaterialLayout(device);
  asset->CreateDescriptorPool(device);
  asset->CreateFallbackResources(device, upload);
  asset->UploadMeshes(device, upload);
  asset->UploadMaterials(device, upload);
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
    if (mat.ownsBaseColorResources) {
      DestroyTexture(device, mat.baseColor);
    }
  }
  materials_.clear();

  DestroyTexture(device, fallbackTexture_);

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

void StaticGltfAsset::Prepare(ICommandList &cmd) { prepared_ = true; }

void StaticGltfAsset::UploadMeshes(IDevice *device, IUploadContext *upload) {
  if (!device) {
    throw std::runtime_error("StaticGltfAsset::UploadMeshes: device is null");
  }

  if (!upload) {
    throw std::runtime_error("StaticGltfAsset::UploadMeshes: upload is null");
  }

  meshes_.clear();
  meshes_.reserve(importedScene_.meshes.size());
  for (const auto &mesh : importedScene_.meshes) {

    auto gpuMesh = MeshUploader::Upload(device, upload, mesh);

    meshes_.push_back(gpuMesh);
  }
}

void StaticGltfAsset::UploadMaterials(IDevice *device, IUploadContext *upload) {
  if (!device) {
    throw std::runtime_error(
        "StaticGltfAsset::UploadMaterials: device is null");
  }

  if (!upload) {
    throw std::runtime_error(
        "StaticGltfAsset::UploadMaterials: upload is null");
  }

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

      gpuMat.baseColor = CreateTexture2D(
          device, upload,
          TextureDesc{.width = static_cast<uint32_t>(img.width),
                      .height = static_cast<uint32_t>(img.height),
                      .format = Format::RGBA8_UNORM,
                      .minFilter = Filter::Linear,
                      .magFilter = Filter::Linear,
                      .addressU = SamplerAddressMode::Repeat,
                      .addressV = SamplerAddressMode::Repeat,
                      .addressW = SamplerAddressMode::Repeat,
                      .debugName = "GLTF Base Color Texture"},
          img.pixelsRGBA8.data(),
          static_cast<uint64_t>(img.pixelsRGBA8.size()));

    } else {
      gpuMat.baseColor = fallbackTexture_;
      gpuMat.ownsBaseColorResources = false;
    }

    gpuMat.descriptorSet =
        device->AllocateDescriptorSet(descriptorPool_, materialLayout_);

    DescriptorImageInfo imageInfo{};
    imageInfo.sampler = gpuMat.baseColor.sampler;
    imageInfo.imageView = gpuMat.baseColor.view;
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
      instance.localTransform = world;

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

void StaticGltfAsset::CreateFallbackResources(IDevice *device,
                                              IUploadContext *upload) {
  if (!device) {
    throw std::runtime_error(
        "StaticGltfAsset::CreateFallbackResources: device is null");
  }

  if (!upload) {
    throw std::runtime_error(
        "StaticGltfAsset::CreateFallbackResources: upload is null");
  }

  if (fallbackTexture_.IsValid()) {
    return;
  }

  const std::uint8_t whitePixel[4] = {255, 255, 255, 255};

  fallbackTexture_ = CreateTexture2D(device, upload,
                                     TextureDesc{
                                         .width = 1,
                                         .height = 1,
                                         .format = Format::RGBA8_UNORM,
                                         .minFilter = Filter::Linear,
                                         .magFilter = Filter::Linear,
                                         .addressU = SamplerAddressMode::Repeat,
                                         .addressV = SamplerAddressMode::Repeat,
                                         .addressW = SamplerAddressMode::Repeat,
                                         .debugName = "Fallback White Texture",
                                     },
                                     whitePixel, 4);
}
} // namespace Rodan
