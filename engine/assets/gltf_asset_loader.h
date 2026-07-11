#pragma once

#include "assets/imported_scene.h"
#include "graphics/material_resource.h"
#include "graphics/texture.h"
#include "rhi/command_list.h"
#include "rhi/upload_context.h"
#include "scene/static_mesh_instance.h"

#include <memory>
#include <rhi/device.h>

namespace Rodan {

using namespace Velos::RHI;

class StaticGltfAsset {
private:
  struct StaticGltfAssetStats {
    uint32_t meshCount = 0;
    uint32_t submeshCount = 0;
    uint32_t instanceCount = 0;
    uint32_t vertexCount = 0;
    uint32_t indexCount = 0;
    uint32_t triangleCount = 0;
    uint32_t materialCount = 0;
    uint32_t textureCount = 0;
  };

public:
  static std::unique_ptr<StaticGltfAsset>
  Load(IDevice *device, IUploadContext *upload, const std::string &path);

  void Prepare(ICommandList &cmd);
  void Destroy(IDevice *device);

  const std::vector<StaticMeshInstance> &GetInstances() const {
    return instances_;
  }

  const std::vector<MaterialResource> &GetMaterials() const {
    return materials_;
  }

  BindingLayoutHandle GetMaterialLayout() const {
    return materialLayout_;
  }

  const StaticGltfAssetStats &GetStats() const { return stats_; }

private:
  IDevice *device_ = nullptr;
  bool fallbackUploaded_ = false;
  bool prepared_ = false;

  std::vector<std::shared_ptr<MeshResource>> meshes_;
  std::vector<MaterialResource> materials_;
  std::vector<StaticMeshInstance> instances_;

  BindingLayoutHandle materialLayout_{};
  BindingPoolHandle bindingPool_{};

  Texture fallbackTexture_{};
  Texture neutralNormalFallbackTexture_{};

  StaticGltfAssetStats stats_;

private:
  void CreateMaterialLayout(IDevice *device);
  void CreateBindingPool(IDevice *device, ImportedScene importedScene);
  void UploadMeshes(IDevice *device, IUploadContext *upload,
                    ImportedScene importedScene);
  void UploadMaterials(IDevice *device, IUploadContext *upload,
                       ImportedScene importedScene);
  void BuildInstances(ImportedScene importedScene);

  void CreateFallbackResources(IDevice *device, IUploadContext *upload);

  void ComputeStats(const ImportedScene &importedScene);
};
} // namespace Rodan
