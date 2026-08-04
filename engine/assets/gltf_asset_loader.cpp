#include "graphics/texture.h"
#include "imgui_internal.h"
#include "rhi/resources.h"
#include "rhi/types.h"
#include "rhi/upload_context.h"
#include "scene/transform.h"
#include <assets/gltf_asset_loader.h>
#include <assets/gltf_loader.h>
#include <graphics/mesh_uploader.h>
#include <iostream>
#include <stdexcept>
#include <graphics/texture_registry.h>

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
  asset->CreateBindingPool(device, importedScene);
  asset->CreateFallbackResources(device, upload);
  asset->BuildMeshResources(device, upload, importedScene);
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

    // Mesh resources only reference the buffers owned by this asset.
    mesh->vertexBuffer = {};
    mesh->indexBuffer = {};
  }
  meshes_.clear();

  if (vertexBuffer_.IsValid()) {
    device->DestroyBuffer(vertexBuffer_);
    vertexBuffer_ = {};
  }

  if (indexBuffer_.IsValid()) {
    device->DestroyBuffer(indexBuffer_);
    indexBuffer_ = {};
  }

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

    if (mat.transmission.ownsTransmissionTexture) {
      DestroyTexture(device, mat.transmission.transmissionTexture);
    }

    if (mat.volume.ownsVolumeTexture) {
      DestroyTexture(device, mat.volume.thicknessTexture);
    }

    if (mat.clearcoat.ownsClearcoatTexture) {
      DestroyTexture(device, mat.clearcoat.texture);
    }

    if (mat.clearcoat.ownsClearcoatRoughnessTexture) {
      DestroyTexture(device, mat.clearcoat.roughnessTexture);
    }

    if (mat.clearcoat.ownsClearcoatNormalTexture) {
      DestroyTexture(device, mat.clearcoat.normalTexture);
    }

    if (mat.emissive.ownsEmissiveTexture) {
      DestroyTexture(device, mat.emissive.texture);
    }
  }

  materials_.clear();

  DestroyTexture(device, fallbackTexture_);
  DestroyTexture(device, neutralNormalFallbackTexture_);

  if (bindingPool_.IsValid()) {
    device->DestroyBindingPool(bindingPool_);
    bindingPool_ = {};
  }

  if (materialLayout_.IsValid()) {
    device->DestroyBindingLayout(materialLayout_);
    materialLayout_ = {};
  }

  prepared_ = false;
  device_ = nullptr;
}

void StaticGltfAsset::CreateMaterialLayout(IDevice *device) {

  BindingDesc bindings[] = {
      {.binding = 0,
       .type = BindingType::CombinedImageSampler,
       .count = 1,
       .visibility = ShaderStage::Fragment},
      {.binding = 1,
       .type = BindingType::CombinedImageSampler,
       .count = 1,
       .visibility = ShaderStage::Fragment},
      {.binding = 2,
       .type = BindingType::CombinedImageSampler,
       .count = 1,
       .visibility = ShaderStage::Fragment},
      {.binding = 3,
       .type = BindingType::CombinedImageSampler,
       .count = 1,
       .visibility = ShaderStage::Fragment},
      {.binding = 4,
       .type = BindingType::CombinedImageSampler,
       .count = 1,
       .visibility = ShaderStage::Fragment}, // transmission
      {.binding = 5,
       .type = BindingType::CombinedImageSampler,
       .count = 1,
       .visibility = ShaderStage::Fragment}, // thickness
      {.binding = 6,
       .type = BindingType::CombinedImageSampler,
       .count = 1,
       .visibility = ShaderStage::Fragment}, // clearcoat texture
      {.binding = 7,
       .type = BindingType::CombinedImageSampler,
       .count = 1,
       .visibility = ShaderStage::Fragment}, // clearcoat roughness
      {.binding = 8,
       .type = BindingType::CombinedImageSampler,
       .count = 1,
       .visibility = ShaderStage::Fragment}, // clearcoat normal
      {.binding = 9,
       .type = BindingType::CombinedImageSampler,
       .count = 1,
       .visibility = ShaderStage::Fragment}, // emissive texture
  };

  BindingLayoutDesc layout{};
  layout.bindings = bindings;
  layout.bindingCount = 10;
  layout.debugName = "GLTF Material Layout";

  materialLayout_ = device->CreateBindingLayout(layout);
}

void StaticGltfAsset::CreateBindingPool(IDevice *device,
                                           ImportedScene importedScene) {

  const Velos::u32 materialCount = std::max<Velos::u32>(
      1, static_cast<Velos::u32>(importedScene.materials.size()));

  BindingPoolSize poolSize{};
  poolSize.type = BindingType::CombinedImageSampler;
  poolSize.count = materialCount * 10;

  BindingPoolDesc poolDesc{};
  poolDesc.poolSizes = &poolSize;
  poolDesc.poolSizeCount = 1;
  poolDesc.maxSets = materialCount;

  bindingPool_ = device->CreateBindingPool(poolDesc);
}

void StaticGltfAsset::Prepare(ICommandList &cmd) { prepared_ = true; }

void StaticGltfAsset::BuildMeshResources(IDevice *device, IUploadContext *upload,
                                   ImportedScene importedScene) {
  if (!device) {
    throw std::runtime_error("StaticGltfAsset::UploadMeshes: device is null");
  }

  if (!upload) {
    throw std::runtime_error("StaticGltfAsset::UploadMeshes: upload is null");
  }

  std::vector<ImportedVertex> combinedVertices;
  std::vector<uint32_t> combinedIndices;

  meshes_.clear();
  meshes_.reserve(importedScene.meshes.size());
  for (const ImportedMesh& mesh : importedScene.meshes) {
      auto resource = std::make_shared<MeshResource>();
      resource->submeshes.reserve(mesh.primitives.size());

      resource->aabb = mesh.localBounds;

      const uint32_t meshFirstVertex = static_cast<uint32_t>(combinedVertices.size());
      const uint32_t meshFirstIndex = static_cast<uint32_t>(combinedIndices.size());

      for (const ImportedPrimitive& primitive : mesh.primitives) {
          const uint32_t baseVertex = static_cast<uint32_t>(combinedVertices.size());

          Submesh submesh{};
          submesh.firstIndex = static_cast<uint32_t>(combinedIndices.size());
          submesh.indexCount = static_cast<uint32_t>(primitive.indices.size());
          submesh.vertexOffset = 0;
          submesh.materialSlot = primitive.materialIndex >= 0 ? static_cast<uint32_t>(primitive.materialIndex) : 0;
          submesh.hasTangents = primitive.hasTangents;
          submesh.aabb = primitive.localBounds;

          combinedVertices.insert(combinedVertices.end(), primitive.vertices.begin(), primitive.vertices.end());
          combinedIndices.reserve(combinedIndices.size() + primitive.indices.size());

          for (uint32_t localIndex : primitive.indices) {
              if (localIndex >= primitive.vertices.size()) {
                  throw std::runtime_error(
                      "StaticGltfAsset::BuildMeshResources: primitive index is out of range");
              }
              combinedIndices.push_back(baseVertex + localIndex);
          }

          resource->submeshes.push_back(submesh);
      }

      resource->firstVertex = meshFirstVertex;
      resource->vertexCount = static_cast<uint32_t>(combinedVertices.size()) - meshFirstVertex;

      resource->firstIndex = meshFirstIndex;
      resource->indexCount = static_cast<uint32_t>(combinedIndices.size()) - meshFirstIndex;

      meshes_.push_back(std::move(resource));
  }

  if (combinedVertices.empty()) {
      throw std::runtime_error(
          "StaticGltfAsset::BuildMeshResources: asset produced no vertices");
  }

  if (combinedIndices.empty()) {
      throw std::runtime_error(
          "StaticGltfAsset::BuildMeshResources: asset produced no indices");
  }

  BufferDesc vertexDesc{
      .size = combinedVertices.size() * sizeof(ImportedVertex),
      .usage = BufferUsage::Vertex | BufferUsage::TransferDst,
      .memoryUsage = MemoryUsage::GPUOnly,
      .debugName = "Static glTF combined vertex buffer",
  };

  BufferDesc indexDesc{
      .size = combinedIndices.size() * sizeof(uint32_t),
      .usage = BufferUsage::Index | BufferUsage::TransferDst,
      .memoryUsage = MemoryUsage::GPUOnly,
      .debugName = "Static glTF combined index buffer",
  };

  vertexBuffer_ = device->CreateBuffer(vertexDesc);
  indexBuffer_ = device->CreateBuffer(indexDesc);

  upload->UploadBuffer({
      .dstBuffer = vertexBuffer_,
      .dstOffset = 0,
      .size = vertexDesc.size,
      .data = combinedVertices.data(),
      });

  upload->UploadBuffer({
      .dstBuffer = indexBuffer_,
      .dstOffset = 0,
      .size = indexDesc.size,
      .data = combinedIndices.data(),
      });

  for (const std::shared_ptr<MeshResource>& mesh : meshes_) {
      mesh->vertexBuffer = vertexBuffer_;
      mesh->indexBuffer = indexBuffer_;
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

  TextureRegistry& textureRegistry = GetTextureRegistry();

  materials_.clear();
  materials_.reserve(importedScene.materials.size());

  for (const auto &mat : importedScene.materials) {
    MaterialResource gpuMat{};

    gpuMat.baseColorFactor = mat.baseColorFactor;
    gpuMat.metallicFactor = mat.metallicFactor;
    gpuMat.roughnessFactor = mat.roughnessFactor;
    gpuMat.alphaCutoff = mat.alphaCutoff;
    gpuMat.alphaMode = mat.alphaMode;
    gpuMat.ior = mat.ior;
    gpuMat.clearcoat.factor = mat.clearCoat.factor;
    gpuMat.clearcoat.roughnessFactor = mat.clearCoat.roughnessFactor;
    gpuMat.emissive.factor = mat.emissive.factor;
    gpuMat.emissive.strength = mat.emissive.emissiveStrength;
    gpuMat.useUnlit = mat.useUnlit;

    gpuMat.baseColorTextureTransformation = mat.baseColorTransformation;
    gpuMat.normalTextureTransformation = mat.normalTransformation;
    gpuMat.metallicRoughnessTextureTransformation =
        mat.metallicRoughnessTransformation;
    gpuMat.occlusionTextureTransformation =
        mat.occlusionTextureTransformation;
    gpuMat.transmission.transmissionTextureTransformation =
        mat.transmission.transmissionTransformation;
    gpuMat.volume.thicknessTextureTransformation =
        mat.volume.thicknessTransformation;
    gpuMat.clearcoat.textureTransformation =
        mat.clearCoat.textureTransformation;
    gpuMat.clearcoat.roughnessTextureTransformation =
        mat.clearCoat.roughnessTransformation;
    gpuMat.clearcoat.normalTextureTransformation =
        mat.clearCoat.normalTransformation;
    gpuMat.emissive.textureTransformation =
        mat.emissive.textureTransformation;

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
      gpuMat.baseColorTextureHandle = textureRegistry.RegisterTexture(gpuMat.baseColorTexture);

    } else {
      gpuMat.baseColorTexture = fallbackTexture_;
      gpuMat.ownsBaseColorResources = false;
      gpuMat.baseColorTextureHandle = TextureHandle{ 0, 1 };
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
      gpuMat.normalTextureHandle = textureRegistry.RegisterTexture(gpuMat.normalTexture);
    } else {
      gpuMat.normalTexture = neutralNormalFallbackTexture_;
      gpuMat.ownsNormalResources = false;
      gpuMat.normalTextureHandle = TextureHandle{ 1, 1 };
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
      gpuMat.metallicRoughnessTextureHandle = textureRegistry.RegisterTexture(gpuMat.metallicRoughnessTexture);

    } else {
      gpuMat.metallicRoughnessTexture = fallbackTexture_;
      gpuMat.ownsMetallicRoughnessResources = false;
      gpuMat.metallicRoughnessTextureHandle = TextureHandle{ 0, 1 };
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
      gpuMat.occlusionTextureHandle = textureRegistry.RegisterTexture(gpuMat.occlusionTexture);
    } else {
      gpuMat.occlusionTexture = fallbackTexture_;
      gpuMat.ownsOcclusionTextureResources = false;
      gpuMat.occlusionTextureHandle = TextureHandle{ 0, 1 };
    }

    gpuMat.transmission.transmissionFactor =
        mat.transmission.transmissionFactor;

    gpuMat.volume.thicknessFactor = mat.volume.thicknessFactor;
    gpuMat.volume.attenuationColor = mat.volume.attenuationColor;
    gpuMat.volume.attenuationDistance = mat.volume.attenuationDistance;

    if (mat.transmission.transmissionTexture.imageIndex >= 0) {
      const ImportedImage &img =
          importedScene.images[mat.transmission.transmissionTexture.imageIndex];

      gpuMat.transmission.transmissionTexture = CreateTexture2D(
          device, upload,
          TextureDesc{.width = static_cast<uint32_t>(img.width),
                      .height = static_cast<uint32_t>(img.height),
                      .format = Format::RGBA8_UNORM,
                      .minFilter = Filter::Linear,
                      .magFilter = Filter::Linear,
                      .addressU = SamplerAddressMode::Repeat,
                      .addressV = SamplerAddressMode::Repeat,
                      .addressW = SamplerAddressMode::Repeat,
                      .debugName = "GLTF Transmission Texture"},
          img.pixelsRGBA8.data(),
          static_cast<uint64_t>(img.pixelsRGBA8.size()));
      gpuMat.transmission.ownsTransmissionTexture = true;
      gpuMat.transmission.transmissionTextureHandle = textureRegistry.RegisterTexture(gpuMat.transmission.transmissionTexture);

    } else {
      gpuMat.transmission.transmissionTexture =
          fallbackTexture_; // white = multiplier 1
      gpuMat.transmission.ownsTransmissionTexture = false;
      gpuMat.transmission.transmissionTextureHandle = TextureHandle{ 0, 1 };
    }

    if (mat.volume.thicknessTexture.imageIndex >= 0) {
      const ImportedImage &img =
          importedScene.images[mat.volume.thicknessTexture.imageIndex];

      gpuMat.volume.thicknessTexture = CreateTexture2D(
          device, upload,
          TextureDesc{.width = static_cast<uint32_t>(img.width),
                      .height = static_cast<uint32_t>(img.height),
                      .format = Format::RGBA8_UNORM,
                      .minFilter = Filter::Linear,
                      .magFilter = Filter::Linear,
                      .addressU = SamplerAddressMode::Repeat,
                      .addressV = SamplerAddressMode::Repeat,
                      .addressW = SamplerAddressMode::Repeat,
                      .debugName = "GLTF Thickness Texture"},
          img.pixelsRGBA8.data(),
          static_cast<uint64_t>(img.pixelsRGBA8.size()));
      gpuMat.volume.ownsVolumeTexture = true;
      gpuMat.volume.thicknessTextureHandle = textureRegistry.RegisterTexture(gpuMat.volume.thicknessTexture);

    } else {
      gpuMat.volume.thicknessTexture = fallbackTexture_; // white = multiplier 1
      gpuMat.volume.thicknessTextureHandle = TextureHandle{ 0, 1 };
    }

    if (mat.clearCoat.texture.imageIndex >= 0) {
      const ImportedImage &img =
          importedScene.images[mat.clearCoat.texture.imageIndex];

      gpuMat.clearcoat.texture = CreateTexture2D(
          device, upload,
          TextureDesc{.width = static_cast<uint32_t>(img.width),
                      .height = static_cast<uint32_t>(img.height),
                      .format = Format::RGBA8_UNORM,
                      .minFilter = Filter::Linear,
                      .magFilter = Filter::Linear,
                      .addressU = SamplerAddressMode::Repeat,
                      .addressV = SamplerAddressMode::Repeat,
                      .addressW = SamplerAddressMode::Repeat,
                      .debugName = "GLTF Clearcoat Texture"},
          img.pixelsRGBA8.data(),
          static_cast<uint64_t>(img.pixelsRGBA8.size()));
      gpuMat.clearcoat.ownsClearcoatTexture = true;
      gpuMat.clearcoat.textureHandle = textureRegistry.RegisterTexture(gpuMat.clearcoat.texture);
    } else {
      gpuMat.clearcoat.texture = fallbackTexture_; // white = multiplier 1
      gpuMat.clearcoat.textureHandle = TextureHandle{ 0, 1 };
    }

    if (mat.clearCoat.roughnessTexture.imageIndex >= 0) {
      const ImportedImage &img =
          importedScene.images[mat.clearCoat.roughnessTexture.imageIndex];

      gpuMat.clearcoat.roughnessTexture = CreateTexture2D(
          device, upload,
          TextureDesc{.width = static_cast<uint32_t>(img.width),
                      .height = static_cast<uint32_t>(img.height),
                      .format = Format::RGBA8_UNORM,
                      .minFilter = Filter::Linear,
                      .magFilter = Filter::Linear,
                      .addressU = SamplerAddressMode::Repeat,
                      .addressV = SamplerAddressMode::Repeat,
                      .addressW = SamplerAddressMode::Repeat,
                      .debugName = "GLTF Clearcoat Roughness Texture"},
          img.pixelsRGBA8.data(),
          static_cast<uint64_t>(img.pixelsRGBA8.size()));
      gpuMat.clearcoat.ownsClearcoatRoughnessTexture = true;
      gpuMat.clearcoat.roughnessTextureHandle = textureRegistry.RegisterTexture(gpuMat.clearcoat.roughnessTexture);
    } else {
      gpuMat.clearcoat.roughnessTexture =
          fallbackTexture_; // white = multiplier 1
      gpuMat.clearcoat.roughnessTextureHandle = TextureHandle{ 0, 1 };
    }

    if (mat.clearCoat.normalTexture.imageIndex >= 0) {
      const ImportedImage &img =
          importedScene.images[mat.clearCoat.normalTexture.imageIndex];

      gpuMat.clearcoat.normalTexture = CreateTexture2D(
          device, upload,
          TextureDesc{.width = static_cast<uint32_t>(img.width),
                      .height = static_cast<uint32_t>(img.height),
                      .format = Format::RGBA8_UNORM,
                      .minFilter = Filter::Linear,
                      .magFilter = Filter::Linear,
                      .addressU = SamplerAddressMode::Repeat,
                      .addressV = SamplerAddressMode::Repeat,
                      .addressW = SamplerAddressMode::Repeat,
                      .debugName = "GLTF Clearcoat Normal Texture"},
          img.pixelsRGBA8.data(),
          static_cast<uint64_t>(img.pixelsRGBA8.size()));
      gpuMat.clearcoat.ownsClearcoatNormalTexture = true;
      gpuMat.clearcoat.normalTextureHandle = textureRegistry.RegisterTexture(gpuMat.clearcoat.normalTexture);
    } else {
      gpuMat.clearcoat.normalTexture =
          neutralNormalFallbackTexture_; // white = multiplier 1
      gpuMat.clearcoat.normalTextureHandle = TextureHandle{ 1, 1 };
    }

    if (mat.emissive.texture.imageIndex >= 0) {
      const ImportedImage &img =
          importedScene.images[mat.emissive.texture.imageIndex];

      gpuMat.emissive.texture = CreateTexture2D(
          device, upload,
          TextureDesc{.width = static_cast<uint32_t>(img.width),
                      .height = static_cast<uint32_t>(img.height),
                      .format = Format::RGBA8_UNORM,
                      .minFilter = Filter::Linear,
                      .magFilter = Filter::Linear,
                      .addressU = SamplerAddressMode::Repeat,
                      .addressV = SamplerAddressMode::Repeat,
                      .addressW = SamplerAddressMode::Repeat,
                      .debugName = "GLTF Clearcoat Normal Texture"},
          img.pixelsRGBA8.data(),
          static_cast<uint64_t>(img.pixelsRGBA8.size()));
      gpuMat.emissive.ownsEmissiveTexture = true;
      gpuMat.emissive.textureHandle = textureRegistry.RegisterTexture(gpuMat.emissive.texture);
    } else {
      gpuMat.emissive.texture = fallbackTexture_; // white = multiplier 1
      gpuMat.emissive.textureHandle = TextureHandle{ 0, 1 };
    }


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
