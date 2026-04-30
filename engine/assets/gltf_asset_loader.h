#pragma once

#include "assets/imported_scene.h"
#include "graphics/material_resource.h"
#include "graphics/texture.h"
#include "rhi/rhi_command_list.h"
#include "rhi/rhi_upload_context.h"
#include "scene/static_mesh_instance.h"

#include <memory>
#include <rhi/rhi_device.h>

namespace Rodan {

using namespace Velos::RHI;

class StaticGltfAsset {
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

  DescriptorSetLayoutHandle GetMaterialLayout() const {
    return materialLayout_;
  }

private:
  IDevice *device_ = nullptr;
  bool fallbackUploaded_ = false;
  bool prepared_ = false;

  std::vector<std::shared_ptr<MeshResource>> meshes_;
  std::vector<MaterialResource> materials_;
  std::vector<StaticMeshInstance> instances_;

  DescriptorSetLayoutHandle materialLayout_{};
  DescriptorPoolHandle descriptorPool_{};

  Texture fallbackTexture_{};

private:
  void CreateMaterialLayout(IDevice *device);
  void CreateDescriptorPool(IDevice *device, ImportedScene importedScene);
  void UploadMeshes(IDevice *device, IUploadContext *upload,
                    ImportedScene importedScene);
  void UploadMaterials(IDevice *device, IUploadContext *upload,
                       ImportedScene importedScene);
  void BuildInstances(ImportedScene importedScene);

  void CreateFallbackResources(IDevice *device, IUploadContext *upload);
};
} // namespace Rodan
