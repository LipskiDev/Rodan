#include "rhi/rhi_resources.h"
#include "rhi/rhi_types.h"
#include "rhi/rhi_upload_context.h"
#include "scene/transform.h"
#include <assets/gltf_asset_loader.h>
#include <assets/gltf_loader.h>
#include <graphics/mesh_uploader.h>
#include <iostream>
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

  auto importedScene = GltfLoader::Load(path);
  asset->CreateMaterialLayout(device);
  asset->CreateDescriptorPool(device, importedScene);
  asset->CreateFallbackResources(device, upload);
  asset->UploadMeshes(device, upload, importedScene);
  asset->UploadMaterials(device, upload, importedScene);
  asset->BuildInstances(importedScene);
  asset->ComputeStats(importedScene);

  return asset;
}

void StaticGltfAsset::Destroy(IDevice *device) {
  if (!device) {
    return;
  }

  instances_.clear();

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
      DestroyTexture(device, mat.baseColorTexture);
    }

    if (mat.ownsNormalResources) {
      DestroyTexture(device, mat.normalTexture);
    }

    if (mat.ownsMetallicRoughnessResources) {
      DestroyTexture(device, mat.metallicRoughnessTexture);
    }

    if (mat.ownsOcclusionTextureResources) {
      DestroyTexture(device, mat.occlusionTexture);
    }
  }
  materials_.clear();

  DestroyTexture(device, fallbackTexture_);
  DestroyTexture(device, neutralNormalFallbackTexture_);

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

  DescriptorBindingDesc bindings[] = {
      DescriptorBindingDesc{
          .binding = 0,
          .type = DescriptorType::CombinedImageSampler,
          .count = 1,
          .visibility = ShaderStage::Fragment,
      },
      DescriptorBindingDesc{
          .binding = 1,
          .type = DescriptorType::CombinedImageSampler,
          .count = 1,
          .visibility = ShaderStage::Fragment,
      },
      DescriptorBindingDesc{
          .binding = 2,
          .type = DescriptorType::CombinedImageSampler,
          .count = 1,
          .visibility = ShaderStage::Fragment,
      },
      DescriptorBindingDesc{
          .binding = 3,
          .type = DescriptorType::CombinedImageSampler,
          .count = 1,
          .visibility = ShaderStage::Fragment,
      },
  };

  DescriptorSetLayoutDesc layout{};
  layout.bindings = bindings;
  layout.bindingCount = 4;
  layout.debugName = "GLTF Material Layout";

  materialLayout_ = device->CreateDescriptorSetLayout(layout);
}

void StaticGltfAsset::CreateDescriptorPool(IDevice *device,
                                           ImportedScene importedScene) {

  const Velos::u32 materialCount = std::max<Velos::u32>(
      1, static_cast<Velos::u32>(importedScene.materials.size()));

  DescriptorPoolSize poolSize{};
  poolSize.type = DescriptorType::CombinedImageSampler;
  poolSize.count = materialCount * 4;

  DescriptorPoolDesc poolDesc{};
  poolDesc.poolSizes = &poolSize;
  poolDesc.poolSizeCount = 1;
  poolDesc.maxSets = materialCount;

  descriptorPool_ = device->CreateDescriptorPool(poolDesc);
}

void StaticGltfAsset::Prepare(ICommandList &cmd) { prepared_ = true; }

void StaticGltfAsset::UploadMeshes(IDevice *device, IUploadContext *upload,
                                   ImportedScene importedScene) {
  if (!device) {
    throw std::runtime_error("StaticGltfAsset::UploadMeshes: device is null");
  }

  if (!upload) {
    throw std::runtime_error("StaticGltfAsset::UploadMeshes: upload is null");
  }

  meshes_.clear();
  meshes_.reserve(importedScene.meshes.size());
  for (const auto &mesh : importedScene.meshes) {

    auto gpuMesh = MeshUploader::Upload(device, upload, mesh);

    meshes_.push_back(gpuMesh);
  }
}

void StaticGltfAsset::UploadMaterials(IDevice *device, IUploadContext *upload,
                                      ImportedScene importedScene) {
  if (!device) {
    throw std::runtime_error(
        "StaticGltfAsset::UploadMaterials: device is null");
  }

  if (!upload) {
    throw std::runtime_error(
        "StaticGltfAsset::UploadMaterials: upload is null");
  }

  materials_.clear();
  materials_.reserve(importedScene.materials.size());

  for (const auto &mat : importedScene.materials) {
    MaterialResource gpuMat{};

    gpuMat.baseColorFactor = mat.baseColorFactor;
    gpuMat.metallicFactor = mat.metallicFactor;
    gpuMat.roughnessFactor = mat.roughnessFactor;
    gpuMat.alphaCutoff = mat.alphaCutoff;
    gpuMat.alphaMode = mat.alphaMode;

    if (mat.baseColorTexture.imageIndex >= 0 &&
        mat.baseColorTexture.imageIndex <
            static_cast<int>(importedScene.images.size())) {
      const ImportedImage &img =
          importedScene.images[mat.baseColorTexture.imageIndex];

      gpuMat.baseColorTexture = CreateTexture2D(
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
      gpuMat.ownsBaseColorResources = true;

    } else {
      gpuMat.baseColorTexture = fallbackTexture_;
      gpuMat.ownsBaseColorResources = false;
    }

    if (mat.normalTexture.imageIndex >= 0 &&
        mat.normalTexture.imageIndex <
            static_cast<int>(importedScene.images.size())) {
      const ImportedImage &img =
          importedScene.images[mat.normalTexture.imageIndex];

      gpuMat.normalTexture = CreateTexture2D(
          device, upload,
          TextureDesc{.width = static_cast<uint32_t>(img.width),
                      .height = static_cast<uint32_t>(img.height),
                      .format = Format::RGBA8_UNORM,
                      .minFilter = Filter::Linear,
                      .magFilter = Filter::Linear,
                      .addressU = SamplerAddressMode::Repeat,
                      .addressV = SamplerAddressMode::Repeat,
                      .addressW = SamplerAddressMode::Repeat,
                      .debugName = "GLTF Normal Texture"},
          img.pixelsRGBA8.data(),
          static_cast<uint64_t>(img.pixelsRGBA8.size()));
      gpuMat.ownsNormalResources = true;
    } else {
      gpuMat.normalTexture = neutralNormalFallbackTexture_;
      gpuMat.ownsNormalResources = false;
    }

    if (mat.metallicRoughnessTexture.imageIndex >= 0 &&
        mat.metallicRoughnessTexture.imageIndex <
            static_cast<int>(importedScene.images.size())) {
      const ImportedImage &img =
          importedScene.images[mat.metallicRoughnessTexture.imageIndex];

      gpuMat.metallicRoughnessTexture = CreateTexture2D(
          device, upload,
          TextureDesc{.width = static_cast<uint32_t>(img.width),
                      .height = static_cast<uint32_t>(img.height),
                      .format = Format::RGBA8_UNORM,
                      .minFilter = Filter::Linear,
                      .magFilter = Filter::Linear,
                      .addressU = SamplerAddressMode::Repeat,
                      .addressV = SamplerAddressMode::Repeat,
                      .addressW = SamplerAddressMode::Repeat,
                      .debugName = "GLTF Metallic Roughness Texture"},
          img.pixelsRGBA8.data(),
          static_cast<uint64_t>(img.pixelsRGBA8.size()));

      gpuMat.ownsMetallicRoughnessResources = true;

    } else {
      gpuMat.metallicRoughnessTexture = fallbackTexture_;
      gpuMat.ownsMetallicRoughnessResources = false;
    }

    if (mat.occlusionTexture.imageIndex >= 0 &&
        mat.occlusionTexture.imageIndex <
            static_cast<int>(importedScene.images.size())) {
      const ImportedImage &img =
          importedScene.images[mat.occlusionTexture.imageIndex];

      gpuMat.occlusionTexture = CreateTexture2D(
          device, upload,
          TextureDesc{.width = static_cast<uint32_t>(img.width),
                      .height = static_cast<uint32_t>(img.height),
                      .format = Format::RGBA8_UNORM,
                      .minFilter = Filter::Linear,
                      .magFilter = Filter::Linear,
                      .addressU = SamplerAddressMode::Repeat,
                      .addressV = SamplerAddressMode::Repeat,
                      .addressW = SamplerAddressMode::Repeat,
                      .debugName = "GLTF Occlusion Texture"},
          img.pixelsRGBA8.data(),
          static_cast<uint64_t>(img.pixelsRGBA8.size()));

      gpuMat.ownsOcclusionTextureResources = true;
    } else {
      gpuMat.occlusionTexture = fallbackTexture_;
      gpuMat.ownsOcclusionTextureResources = false;
    }

    gpuMat.transmission.transmissionFactor =
        mat.transmission.transmissionFactor;
    gpuMat.transmission.transmissionTexture =
        mat.transmission.transmissionTexture;

    gpuMat.descriptorSet =
        device->AllocateDescriptorSet(descriptorPool_, materialLayout_);

    DescriptorImageInfo baseColorImageInfo{};
    baseColorImageInfo.sampler = gpuMat.baseColorTexture.sampler;
    baseColorImageInfo.imageView = gpuMat.baseColorTexture.view;
    baseColorImageInfo.imageLayout = ImageLayout::ShaderReadOnly;

    DescriptorImageInfo normalImageInfo{};
    normalImageInfo.sampler = gpuMat.normalTexture.sampler;
    normalImageInfo.imageView = gpuMat.normalTexture.view;
    normalImageInfo.imageLayout = ImageLayout::ShaderReadOnly;

    DescriptorImageInfo metallicRoughnessImageInfo{};
    metallicRoughnessImageInfo.sampler =
        gpuMat.metallicRoughnessTexture.sampler;
    metallicRoughnessImageInfo.imageView = gpuMat.metallicRoughnessTexture.view;
    metallicRoughnessImageInfo.imageLayout = ImageLayout::ShaderReadOnly;

    DescriptorImageInfo occlusionImageInfo{};
    occlusionImageInfo.sampler = gpuMat.occlusionTexture.sampler;
    occlusionImageInfo.imageView = gpuMat.occlusionTexture.view;
    occlusionImageInfo.imageLayout = ImageLayout::ShaderReadOnly;

    device->UpdateDescriptorSet({
        .dstSet = gpuMat.descriptorSet,
        .binding = 0,
        .arrayElement = 0,
        .type = DescriptorType::CombinedImageSampler,
        .bufferInfo = nullptr,
        .imageInfo = &baseColorImageInfo,
        .descriptorCount = 1,
    });

    device->UpdateDescriptorSet({
        .dstSet = gpuMat.descriptorSet,
        .binding = 1,
        .arrayElement = 0,
        .type = DescriptorType::CombinedImageSampler,
        .bufferInfo = nullptr,
        .imageInfo = &normalImageInfo,
        .descriptorCount = 1,
    });

    device->UpdateDescriptorSet({
        .dstSet = gpuMat.descriptorSet,
        .binding = 2,
        .arrayElement = 0,
        .type = DescriptorType::CombinedImageSampler,
        .bufferInfo = nullptr,
        .imageInfo = &metallicRoughnessImageInfo,
        .descriptorCount = 1,
    });

    device->UpdateDescriptorSet({
        .dstSet = gpuMat.descriptorSet,
        .binding = 3,
        .arrayElement = 0,
        .type = DescriptorType::CombinedImageSampler,
        .bufferInfo = nullptr,
        .imageInfo = &occlusionImageInfo,
        .descriptorCount = 1,
    });

    materials_.push_back(gpuMat);
  }
}

void StaticGltfAsset::BuildInstances(ImportedScene importedScene) {
  std::function<void(int, const Transform &)> spawn;

  spawn = [&](int nodeIndex, const Transform &parentTransform) {
    const auto &node = importedScene.nodes[nodeIndex];

    const Transform localTransform = node.localTransform;
    const Transform worldTransform = Transform::FromMatrix(
        parentTransform.ToMatrix() * localTransform.ToMatrix());

    if (node.meshIndex >= 0) {
      if (node.meshIndex >= static_cast<int>(meshes_.size())) {
        throw std::runtime_error("StaticGltfAsset: invalid mesh index in node");
      }

      std::cout << "node=" << node.name << " meshIndex=" << node.meshIndex
                << " mesh=" << importedScene.meshes[node.meshIndex].name
                << " worldPos=(" << worldTransform.position.x << ", "
                << worldTransform.position.y << ", "
                << worldTransform.position.z << ")" << std::endl;

      StaticMeshInstance instance{};
      instance.mesh = meshes_[node.meshIndex];
      instance.localTransform = node.worldTransform;
      instance.worldTransform = Transform{};

      instances_.push_back(instance);
    }

    for (int child : node.children) {
      spawn(child, worldTransform);
    }
  };

  for (int root : importedScene.rootNodes) {
    spawn(root, Transform{});
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

  const std::uint8_t neutralNormal[4] = {128, 128, 255, 255};

  neutralNormalFallbackTexture_ =
      CreateTexture2D(device, upload,
                      TextureDesc{
                          .width = 1,
                          .height = 1,
                          .format = Format::RGBA8_UNORM,
                          .minFilter = Filter::Linear,
                          .magFilter = Filter::Linear,
                          .addressU = SamplerAddressMode::Repeat,
                          .addressV = SamplerAddressMode::Repeat,
                          .addressW = SamplerAddressMode::Repeat,
                          .debugName = "Fallback Neutral Normal Texture",
                      },
                      neutralNormal, 4);
}

void StaticGltfAsset::ComputeStats(const ImportedScene &importedScene) {
  stats_ = {};

  stats_.meshCount = static_cast<uint32_t>(meshes_.size());
  stats_.instanceCount = static_cast<uint32_t>(instances_.size());
  stats_.materialCount = static_cast<uint32_t>(materials_.size());
  stats_.textureCount = static_cast<uint32_t>(importedScene.images.size());

  for (const auto &mesh : meshes_) {
    if (!mesh) {
      continue;
    }

    stats_.vertexCount += mesh->vertexCount;
    stats_.indexCount += mesh->indexCount;

    for (const Submesh &submesh : mesh->submeshes) {
      stats_.triangleCount += submesh.indexCount / 3;
      stats_.submeshCount++;
    }
  }
}
} // namespace Rodan
